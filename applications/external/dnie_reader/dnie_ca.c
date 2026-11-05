#include "dnie_ca.h"
#include "dnie_crypto.h"
#include "dnie_pace_math.h"

#include <string.h>
#include <furi_hal_random.h>

#define TAG      "DnieCa"
#define SW_OK    0x9000
#define CA_STACK (8 * 1024) // mbedtls ECP en un hilo con pila grande, como en PACE

typedef struct {
    // entrada
    const DnieCaKey* key;
    // salida
    int ret;
    uint8_t pk_eph[DNIE_EC_POINT_LEN];
    uint8_t k[DNIE_EC_COORD_LEN];
} CaEcc;

static int ca_rng(void* ctx, unsigned char* buf, size_t len) {
    UNUSED(ctx);
    furi_hal_random_fill_buf(buf, len);
    return 0;
}

static int ca_ecc(CaEcc* c) {
    mbedtls_ecp_group grp;
    mbedtls_mpi sk;
    mbedtls_ecp_point pk, chip;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&sk);
    mbedtls_ecp_point_init(&pk);
    mbedtls_ecp_point_init(&chip);
    int ret;
    do {
        // Solo brainpoolP256r1 (la del DNIe): cargar NIST P-256 arrastraria ~4 KB de codigo
        if((ret = dnie_ec_load_bp256r1(&grp))) break;
        // Clave estatica del chip (de DG14): tiene que ser un punto valido de la curva
        if((ret = dnie_ec_point_read(&grp, &chip, c->key->public_key, c->key->public_key_len)))
            break;
        // Par efimero y secreto compartido
        if((ret = dnie_pace_keypair(&grp, &grp.G, &sk, &pk, true, ca_rng, NULL))) break;
        if((ret = dnie_ec_point_write(&grp, &pk, c->pk_eph))) break;
        ret = dnie_pace_shared_x(&grp, &sk, &chip, c->k, ca_rng, NULL);
    } while(0);
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&sk);
    mbedtls_ecp_point_free(&pk);
    mbedtls_ecp_point_free(&chip);
    return ret;
}

static int32_t ca_thread(void* context) {
    CaEcc* c = context;
    c->ret = ca_ecc(c);
    return 0;
}

DnieCaResult dnie_ca_run(
    DnieTransport* poller,
    DnieSm* sm,
    const DnieDg14* dg14,
    const uint8_t dg1_head[4],
    DnieCaStatus* st) {
    memset(st, 0, sizeof(*st));

    // 1. Elegir protocolo (ECDH + 3DES o AES-128) y la clave correspondiente
    const DnieCaInfo* info = NULL;
    for(uint8_t i = 0; i < dg14->info_count && !info; i++) {
        const DnieCaInfo* ci = &dg14->infos[i];
        if(ci->ka == 2 && (ci->cipher == 1 || ci->cipher == 2)) info = ci;
    }
    const DnieCaKey* key = NULL;
    for(uint8_t i = 0; i < dg14->key_count && !key; i++) {
        const DnieCaKey* k = &dg14->keys[i];
        if(k->ka != 2) continue;
        if(info && info->has_key_id && k->has_key_id && k->key_id != info->key_id) continue;
        key = k;
    }
    if(!info && !key) return st->result = DnieCaNotAvailable;
    if(!info || !key) return st->result = DnieCaUnsupported;
    if(key->curve != DnieCurveBrainpoolP256r1) return st->result = DnieCaUnsupported;
    if(key->public_key_len != DNIE_EC_POINT_LEN) return st->result = DnieCaUnsupported;
    st->cipher = info->cipher;

    // 2. Criptografia de curva eliptica en un hilo con pila grande (si hay memoria para ella)
    if(memmgr_heap_get_max_free_block() < CA_STACK + 2048 ||
       memmgr_get_free_heap() < CA_STACK + 8192) {
        FURI_LOG_E(TAG, "Sin memoria para CA (libre %u)", memmgr_get_free_heap());
        return st->result = DnieCaNoMemory;
    }
    CaEcc* c = malloc(sizeof(CaEcc));
    if(!c) return st->result = DnieCaCrypto;
    memset(c, 0, sizeof(*c));
    c->key = key;
    c->ret = -1;
    uint32_t t0 = furi_get_tick();
    FuriThread* th = furi_thread_alloc_ex("DnieCa", CA_STACK, ca_thread, c);
    furi_thread_start(th);
    furi_thread_join(th);
    furi_thread_free(th);
    st->ecc_ms = furi_get_tick() - t0;
    FURI_LOG_I(TAG, "ECC CA: %lu ms, ret %d", st->ecc_ms, c->ret);
    if(c->ret != 0) {
        memset(c, 0, sizeof(*c));
        free(c);
        return st->result = DnieCaCrypto;
    }

    do {
        // 3. MSE:Set KAT: 91 = clave publica efimera, 84 = referencia de la clave (si la hay)
        uint8_t data[2 + DNIE_EC_POINT_LEN + 6];
        size_t n = 0;
        data[n++] = 0x91;
        data[n++] = DNIE_EC_POINT_LEN;
        memcpy(&data[n], c->pk_eph, DNIE_EC_POINT_LEN);
        n += DNIE_EC_POINT_LEN;
        if(info->has_key_id || key->has_key_id) {
            uint32_t id = info->has_key_id ? info->key_id : key->key_id;
            data[n++] = 0x84;
            data[n++] = 0x01;
            data[n++] = (uint8_t)id;
        }
        const uint8_t kat[4] = {0x00, 0x22, 0x41, 0xA6};
        uint8_t resp[16];
        size_t rlen;
        DnieLdsStatus ls;
        if(!dnie_lds_sm_command(poller, sm, kat, data, n, -1, resp, sizeof(resp), &rlen, &ls)) {
            st->sw = ls.sw;
            st->lds = ls;
            st->result = ls.result == DnieLdsErrorTransport ? DnieCaTransport : DnieCaRejected;
            break;
        }
        st->sw = ls.sw;
        if(ls.sw != SW_OK) {
            st->result = DnieCaRejected;
            break;
        }

        // 4. Claves nuevas: el chip ya ha cambiado; nosotros tambien (SSC = 0)
        uint8_t ks_enc[16], ks_mac[16];
        bool ok;
        if(info->cipher == 1) {
            ok = dnie_kdf_3des(c->k, DNIE_EC_COORD_LEN, DNIE_KDF_ENC, ks_enc) &&
                 dnie_kdf_3des(c->k, DNIE_EC_COORD_LEN, DNIE_KDF_MAC, ks_mac);
            if(ok) dnie_sm_init_3des(sm, ks_enc, ks_mac, NULL);
        } else {
            ok = dnie_kdf_aes128(c->k, DNIE_EC_COORD_LEN, DNIE_KDF_ENC, ks_enc) &&
                 dnie_kdf_aes128(c->k, DNIE_EC_COORD_LEN, DNIE_KDF_MAC, ks_mac);
            if(ok) dnie_sm_init(sm, ks_enc, ks_mac);
        }
        memset(ks_enc, 0, sizeof(ks_enc));
        memset(ks_mac, 0, sizeof(ks_mac));
        if(!ok) {
            st->result = DnieCaCrypto;
            break;
        }

        // 5. Comprobacion: volver a leer el principio de DG1 por el canal nuevo.
        //    Un clon no puede calcular K (no tiene la clave privada) y fallara el MAC.
        const uint8_t sel[4] = {0x00, 0xA4, 0x02, 0x0C};
        const uint8_t fid[2] = {0x01, 0x01};
        if(!dnie_lds_sm_command(poller, sm, sel, fid, 2, -1, resp, sizeof(resp), &rlen, &st->lds)) {
            st->result = st->lds.result == DnieLdsErrorTransport ? DnieCaTransport : DnieCaFailed;
            break;
        }
        const uint8_t rb[4] = {0x00, 0xB0, 0x00, 0x00};
        if(!dnie_lds_sm_command(poller, sm, rb, NULL, 0, 4, resp, sizeof(resp), &rlen, &st->lds)) {
            st->result = st->lds.result == DnieLdsErrorTransport ? DnieCaTransport : DnieCaFailed;
            break;
        }
        st->sw = st->lds.sw;
        st->result = (st->lds.sw == SW_OK && rlen == 4 && memcmp(resp, dg1_head, 4) == 0) ?
                         DnieCaOk :
                         DnieCaFailed;
    } while(0);

    memset(c, 0, sizeof(*c));
    free(c);
    return st->result;
}

const char* dnie_ca_result_str(DnieCaResult r) {
    switch(r) {
    case DnieCaOk:
        return "GENUINE CHIP";
    case DnieCaNotAvailable:
        return "not available";
    case DnieCaUnsupported:
        return "unsupported variant";
    case DnieCaRejected:
        return "rejected by chip";
    case DnieCaFailed:
        return "FAIL: possible clone";
    case DnieCaTransport:
        return "radio error";
    case DnieCaCrypto:
        return "crypto error";
    case DnieCaNoMemory:
        return "out of memory";
    default:
        return "not performed";
    }
}
