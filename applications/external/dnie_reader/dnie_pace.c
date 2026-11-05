#include "dnie_pace.h"
#include "dnie_crypto.h"
#include "dnie_pace_math.h"

#include <string.h>
#include <furi_hal_random.h>
#include <mbedtls/ecp.h>
#include <mbedtls/bignum.h>

#define TAG   "DniePace"
#define SW_OK 0x9000

#define PACE_MAPPING_ECDH_GM 2
#define PACE_CIPHER_AES128   2
#define PACE_PARAM_BP256R1   13
#define PACE_PASSWORD_CAN    0x02

#define DNIE_ECC_STACK_SIZE (8 * 1024) // mbedtls ECP usa bastante pila
#define DNIE_RX_SIZE        160

static const uint8_t ID_PACE_PREFIX[] = {0x04, 0x00, 0x7F, 0x00, 0x07, 0x02, 0x02, 0x04};

// ---------------------------------------------------------------------------
// Curva eliptica en un hilo con pila grande
// ---------------------------------------------------------------------------

typedef enum {
    DnieEccOpMapKeypair, // carga curva, genera (sk_map, PK_map)
    DnieEccOpMapAndEphemeral, // G~ = s*G + sk_map*PK_map_chip ; genera (sk_eph, PK_eph) sobre G~
    DnieEccOpSharedSecret, // K = x(sk_eph * PK_chip) ; KSenc, KSmac
} DnieEccOp;

typedef struct {
    DnieEccOp op;
    int ret;
    mbedtls_ecp_group grp;
    mbedtls_mpi sk_map, sk_eph;
    mbedtls_ecp_point g_tilde;
    const uint8_t* s;
    uint8_t pk_map[DNIE_EC_POINT_LEN]; // nuestro PK de mapeo
    uint8_t pk_map_chip[DNIE_EC_POINT_LEN]; // PK de mapeo del chip
    uint8_t pk_eph[DNIE_EC_POINT_LEN]; // nuestro PK efimero
    uint8_t pk_chip[DNIE_EC_POINT_LEN]; // PK efimero del chip
    uint8_t ks_enc[16];
    uint8_t ks_mac[16];
} DnieEccCtx;

static int dnie_rng(void* ctx, unsigned char* buf, size_t len) {
    UNUSED(ctx);
    furi_hal_random_fill_buf(buf, len); // TRNG del STM32WB55
    return 0;
}

static int dnie_ecc_run_op(DnieEccCtx* c) {
    int ret = 0;
    mbedtls_ecp_point a, b;
    mbedtls_ecp_point_init(&a);
    mbedtls_ecp_point_init(&b);

    switch(c->op) {
    case DnieEccOpMapKeypair:
        if((ret = dnie_ec_load_bp256r1(&c->grp)) != 0) break;
        if((ret = dnie_pace_keypair(&c->grp, &c->grp.G, &c->sk_map, &a, true, dnie_rng, NULL)) !=
           0)
            break;
        ret = dnie_ec_point_write(&c->grp, &a, c->pk_map);
        break;

    case DnieEccOpMapAndEphemeral:
        if((ret = dnie_ec_point_read(&c->grp, &a, c->pk_map_chip, DNIE_EC_POINT_LEN)) != 0) break;
        if((ret = dnie_pace_map_gm(
                &c->grp, c->s, DNIE_NONCE_LEN, &c->sk_map, &a, &c->g_tilde, dnie_rng, NULL)) != 0)
            break;
        if((ret = dnie_pace_keypair(&c->grp, &c->g_tilde, &c->sk_eph, &b, true, dnie_rng, NULL)) !=
           0)
            break;
        ret = dnie_ec_point_write(&c->grp, &b, c->pk_eph);
        break;

    case DnieEccOpSharedSecret: {
        uint8_t k[DNIE_EC_COORD_LEN];
        // TR-03110: el PK efimero del chip no puede repetir su PK de mapeo
        if(memcmp(c->pk_chip, c->pk_map_chip, DNIE_EC_POINT_LEN) == 0) {
            ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            break;
        }
        if((ret = dnie_ec_point_read(&c->grp, &a, c->pk_chip, DNIE_EC_POINT_LEN)) != 0) break;
        if((ret = dnie_pace_shared_x(&c->grp, &c->sk_eph, &a, k, dnie_rng, NULL)) != 0) break;
        if(!dnie_pace_derive_keys(k, c->ks_enc, c->ks_mac)) ret = -1;
        memset(k, 0, sizeof(k));
        break;
    }
    }

    mbedtls_ecp_point_free(&a);
    mbedtls_ecp_point_free(&b);
    return ret;
}

static int32_t dnie_ecc_thread(void* context) {
    DnieEccCtx* c = context;
    c->ret = dnie_ecc_run_op(c);
    return 0;
}

// Ejecuta la operacion en un hilo aparte y espera. Devuelve el ret de mbedtls.
// -2: sin hueco para el hilo. Los ALLOC_FAILED de mbedtls tambien son falta de memoria.
static DniePaceResult ecc_error(int ret, DniePaceResult otherwise) {
    if(ret == -2 || ret == MBEDTLS_ERR_ECP_ALLOC_FAILED || ret == MBEDTLS_ERR_MPI_ALLOC_FAILED)
        return DniePaceErrorNoMemory;
    return otherwise;
}

static int dnie_ecc_run(DnieEccCtx* c, DnieEccOp op, uint32_t* elapsed_ms) {
    c->op = op;
    c->ret = -1;
    // Sin un hueco para la pila del hilo (+ margen para mbedtls) el malloc colgaria el Flipper
    if(memmgr_heap_get_max_free_block() < DNIE_ECC_STACK_SIZE + 2048 ||
       memmgr_get_free_heap() < DNIE_ECC_STACK_SIZE + 8192) {
        FURI_LOG_E(
            TAG,
            "Sin memoria para ECC (libre %u, bloque %u)",
            memmgr_get_free_heap(),
            memmgr_heap_get_max_free_block());
        return -2;
    }
    uint32_t t0 = furi_get_tick();
    FuriThread* thread = furi_thread_alloc_ex("DnieEcc", DNIE_ECC_STACK_SIZE, dnie_ecc_thread, c);
    furi_thread_start(thread);
    furi_thread_join(thread);
    furi_thread_free(thread);
    uint32_t dt = furi_get_tick() - t0;
    *elapsed_ms += dt;
    FURI_LOG_I(TAG, "ECC op %d: %lu ms, ret %d", op, dt, c->ret);
    return c->ret;
}

// ---------------------------------------------------------------------------
// APDUs
// ---------------------------------------------------------------------------

bool dnie_pace_choose(const DniePaceInfo* infos, size_t n, DniePaceInfo* chosen) {
    for(size_t i = 0; i < n; i++) {
        if(infos[i].mapping == PACE_MAPPING_ECDH_GM && infos[i].cipher == PACE_CIPHER_AES128 &&
           infos[i].has_param_id && infos[i].param_id == PACE_PARAM_BP256R1) {
            *chosen = infos[i];
            return true;
        }
    }
    return false;
}

static void dnie_pace_oid(const DniePaceInfo* info, uint8_t oid[DNIE_PACE_OID_LEN]) {
    memcpy(oid, ID_PACE_PREFIX, sizeof(ID_PACE_PREFIX));
    oid[8] = info->mapping;
    oid[9] = info->cipher;
}

// General Authenticate: CLA 86 00 00 Lc | 7C L | tag len data | Le=00
// Extrae de la respuesta el objeto resp_tag, que debe medir exactamente out_len.
// Devuelve false si hubo error de transporte; si no, mira *sw y *found.
static bool dnie_pace_ga(
    DnieTransport* poller,
    BitBuffer* tx,
    BitBuffer* rx,
    bool chained, // true = quedan mas GA despues (CLA 0x10)
    uint8_t tag,
    const uint8_t* data,
    size_t len,
    uint8_t resp_tag,
    uint8_t* out,
    size_t out_len,
    uint16_t* sw,
    bool* found) {
    uint8_t apdu[5 + 2 + 2 + DNIE_EC_POINT_LEN + 1];
    size_t i = 0;
    apdu[i++] = chained ? 0x10 : 0x00;
    apdu[i++] = 0x86;
    apdu[i++] = 0x00;
    apdu[i++] = 0x00;
    apdu[i++] = (uint8_t)(len == 0 ? 2 : 4 + len); // Lc
    apdu[i++] = 0x7C;
    apdu[i++] = (uint8_t)(len == 0 ? 0 : 2 + len);
    if(len > 0) {
        apdu[i++] = tag;
        apdu[i++] = (uint8_t)len;
        memcpy(&apdu[i], data, len);
        i += len;
    }
    apdu[i++] = 0x00; // Le

    *found = false;
    if(!dnie_card_apdu(poller, apdu, i, tx, rx, sw)) return false;
    if(*sw != SW_OK) return true;

    // Respuesta: 7C L { tag len value ... } SW1 SW2
    size_t n = bit_buffer_get_size_bytes(rx) - 2;
    const uint8_t* d = bit_buffer_get_data(rx);
    if(n < 2 || d[0] != 0x7C) return true;
    size_t pos = 2, end = 2 + d[1];
    if(d[1] == 0x81 && n >= 3) {
        pos = 3;
        end = 3 + d[2];
    }
    if(end > n) return true;

    while(pos + 2 <= end) {
        uint8_t t = d[pos], l = d[pos + 1];
        if(pos + 2 + l > end) break;
        if(t == resp_tag && l == out_len) {
            memcpy(out, &d[pos + 2], out_len);
            *found = true;
            break;
        }
        pos += 2 + l;
    }
    return true;
}

// Paso 1: MSE:Set AT + nonce cifrado + descifrado con K_pi = KDF(CAN, 3)
static DniePaceResult dnie_pace_step_nonce(
    DnieTransport* poller,
    const DniePacePassword* pw,
    DniePaceState* state,
    BitBuffer* tx,
    BitBuffer* rx) {
    uint8_t oid[DNIE_PACE_OID_LEN];
    dnie_pace_oid(&state->info, oid);

    // 80: OID   83: tipo de contrasena (02 = CAN)   84: parameterId
    uint8_t mse[5 + 2 + DNIE_PACE_OID_LEN + 3 + 3] = {0x00, 0x22, 0xC1, 0xA4, 0x12, 0x80, 0x0A};
    memcpy(&mse[7], oid, DNIE_PACE_OID_LEN);
    const uint8_t tail[] = {0x83, 0x01, pw->ref, 0x84, 0x01, (uint8_t)state->info.param_id};
    memcpy(&mse[7 + DNIE_PACE_OID_LEN], tail, sizeof(tail));

    if(!dnie_card_apdu(poller, mse, sizeof(mse), tx, rx, &state->sw))
        return DniePaceErrorTransport;
    if(state->sw != SW_OK) return DniePaceErrorMse;

    uint8_t z[DNIE_NONCE_LEN];
    bool found;
    if(!dnie_pace_ga(poller, tx, rx, true, 0, NULL, 0, 0x80, z, sizeof(z), &state->sw, &found))
        return DniePaceErrorTransport;
    if(state->sw != SW_OK) return DniePaceErrorNonce;
    if(!found) return DniePaceErrorFormat;

    uint8_t k_pi[16];
    bool ok = dnie_kdf_aes128(pw->secret, pw->len, DNIE_KDF_PI, k_pi) &&
              dnie_aes128_cbc_decrypt_iv0(k_pi, z, DNIE_NONCE_LEN, state->nonce);
    memset(k_pi, 0, sizeof(k_pi));
    return ok ? DniePaceOk : DniePaceErrorCrypto;
}

DniePaceResult dnie_pace_establish(
    DnieTransport* poller,
    const DniePacePassword* pw,
    DniePaceState* state,
    DniePaceProgressCallback progress,
    void* progress_context) {
    BitBuffer* tx = bit_buffer_alloc(DNIE_RX_SIZE);
    BitBuffer* rx = bit_buffer_alloc(DNIE_RX_SIZE);
    DnieEccCtx* c = malloc(sizeof(DnieEccCtx));
    memset(c, 0, sizeof(*c));
    mbedtls_ecp_group_init(&c->grp);
    mbedtls_mpi_init(&c->sk_map);
    mbedtls_mpi_init(&c->sk_eph);
    mbedtls_ecp_point_init(&c->g_tilde);

    DniePaceResult result = DniePaceOk;
    uint8_t oid[DNIE_PACE_OID_LEN];
    dnie_pace_oid(&state->info, oid);
    bool found;

    do {
        // ---- Paso 1: nonce ----
        int ecc_ret;
        state->step = 1;
        if(progress) progress(1, progress_context);
        result = dnie_pace_step_nonce(poller, pw, state, tx, rx);
        if(result != DniePaceOk) break;

        // ---- Paso 2: mapeo del generador ----
        state->step = 2;
        if(progress) progress(2, progress_context);
        if((ecc_ret = dnie_ecc_run(c, DnieEccOpMapKeypair, &state->ecc_ms)) != 0) {
            result = ecc_error(ecc_ret, DniePaceErrorCrypto);
            break;
        }
        if(!dnie_pace_ga(
               poller,
               tx,
               rx,
               true,
               0x81,
               c->pk_map,
               DNIE_EC_POINT_LEN,
               0x82,
               c->pk_map_chip,
               DNIE_EC_POINT_LEN,
               &state->sw,
               &found)) {
            result = DniePaceErrorTransport;
            break;
        }
        if(state->sw != SW_OK) {
            result = DniePaceErrorMap;
            break;
        }
        if(!found) {
            result = DniePaceErrorFormat;
            break;
        }

        // ---- Paso 3: ECDH sobre G~ ----
        state->step = 3;
        if(progress) progress(3, progress_context);
        c->s = state->nonce;
        if((ecc_ret = dnie_ecc_run(c, DnieEccOpMapAndEphemeral, &state->ecc_ms)) != 0) {
            result = ecc_error(ecc_ret, DniePaceErrorBadPoint);
            break;
        }
        if(!dnie_pace_ga(
               poller,
               tx,
               rx,
               true,
               0x83,
               c->pk_eph,
               DNIE_EC_POINT_LEN,
               0x84,
               c->pk_chip,
               DNIE_EC_POINT_LEN,
               &state->sw,
               &found)) {
            result = DniePaceErrorTransport;
            break;
        }
        if(state->sw != SW_OK) {
            result = DniePaceErrorKeyAgreement;
            break;
        }
        if(!found) {
            result = DniePaceErrorFormat;
            break;
        }
        if((ecc_ret = dnie_ecc_run(c, DnieEccOpSharedSecret, &state->ecc_ms)) != 0) {
            result = ecc_error(ecc_ret, DniePaceErrorBadPoint);
            break;
        }

        // ---- Paso 4: tokens de autenticacion mutua ----
        state->step = 4;
        if(progress) progress(4, progress_context);
        uint8_t t_pcd[DNIE_PACE_TOKEN_LEN], t_chip[DNIE_PACE_TOKEN_LEN],
            t_exp[DNIE_PACE_TOKEN_LEN];
        if(!dnie_pace_token(c->ks_mac, oid, c->pk_chip, t_pcd) ||
           !dnie_pace_token(c->ks_mac, oid, c->pk_eph, t_exp)) {
            result = DniePaceErrorCrypto;
            break;
        }
        // Ultimo GA: CLA 00 (fin del encadenamiento)
        if(!dnie_pace_ga(
               poller,
               tx,
               rx,
               false,
               0x85,
               t_pcd,
               sizeof(t_pcd),
               0x86,
               t_chip,
               sizeof(t_chip),
               &state->sw,
               &found)) {
            result = DniePaceErrorTransport;
            break;
        }
        if(state->sw != SW_OK) {
            // El chip no acepta nuestro token: 63xx (autenticacion fallida) o 6988 (objetos SM
            // incorrectos, lo que devuelven algunos pasaportes). Las dos cosas significan que
            // la contrasena (CAN o datos de la MRZ) no coincide.
            result = ((state->sw & 0xFF00) == 0x6300 || state->sw == 0x6988) ?
                         DniePaceErrorWrongCan :
                         DniePaceErrorChipToken;
            break;
        }
        if(!found) {
            result = DniePaceErrorFormat;
            break;
        }
        if(memcmp(t_chip, t_exp, DNIE_PACE_TOKEN_LEN) != 0) {
            result = DniePaceErrorChipToken;
            break;
        }

        memcpy(state->ks_enc, c->ks_enc, 16);
        memcpy(state->ks_mac, c->ks_mac, 16);
    } while(false);

    // Limpieza (incluye borrar secretos)
    mbedtls_ecp_group_free(&c->grp);
    mbedtls_mpi_free(&c->sk_map);
    mbedtls_mpi_free(&c->sk_eph);
    mbedtls_ecp_point_free(&c->g_tilde);
    memset(c, 0, sizeof(*c));
    free(c);
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    if(result != DniePaceOk) {
        memset(state->ks_enc, 0, 16);
        memset(state->ks_mac, 0, 16);
    }
    return result;
}

const char* dnie_pace_result_str(DniePaceResult result) {
    switch(result) {
    case DniePaceOk:
        return "OK";
    case DniePaceErrorUnsupported:
        return "PACE variant\nnot supported";
    case DniePaceErrorTransport:
        return "Radio error\n(don't move the ID)";
    case DniePaceErrorMse:
        return "Chip rejected\nMSE:Set AT";
    case DniePaceErrorNonce:
        return "Chip rejected\nstep 1 (nonce)";
    case DniePaceErrorMap:
        return "Chip rejected\nstep 2 (mapping)";
    case DniePaceErrorKeyAgreement:
        return "Chip rejected\nstep 3 (ECDH)";
    case DniePaceErrorWrongCan:
        return "Wrong CAN or\nMRZ data";
    case DniePaceErrorChipToken:
        return "Chip token\nmismatch";
    case DniePaceErrorFormat:
        return "Response has\nunexpected format";
    case DniePaceErrorBadPoint:
        return "Invalid curve\npoint";
    case DniePaceErrorCrypto:
        return "Crypto error";
    case DniePaceErrorNoMemory:
        return "No memory for\nelliptic curve";
    }
    return "Unknown error";
}
