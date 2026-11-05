#include "dnie_auth.h"
#include <storage/storage.h>

#include <string.h>

#define TAG "DnieAuth"

typedef struct {
    uint8_t* buf;
    size_t max;
    size_t len;
} MemSink;

static bool mem_sink(const uint8_t* data, size_t len, void* context) {
    MemSink* m = context;
    if(m->len + len > m->max) return false;
    memcpy(&m->buf[m->len], data, len);
    m->len += len;
    return true;
}

static size_t csca_file_read(void* ctx, uint8_t* buf, size_t len) {
    return storage_file_read((File*)ctx, buf, len);
}

void dnie_auth_reset(DnieAuth* a) {
    free(a->sod_buf);
    free(a->sod);
    free(a->dg14_buf);
    memset(a, 0, sizeof(*a));
}

static DnieCheck check_hash(const DnieSod* sod, uint8_t dg, const uint8_t* data, size_t len) {
    const uint8_t* expected = dnie_sod_dg_hash(sod, dg);
    if(!expected) return DnieCheckMissing;
    if(!dnie_hash_supported(sod->lds_hash)) return DnieCheckUnsupported;
    uint8_t h[DNIE_HASH_MAX];
    if(!dnie_hash(sod->lds_hash, data, len, h)) return DnieCheckUnsupported;
    return memcmp(h, expected, dnie_hash_len(sod->lds_hash)) == 0 ? DnieCheckOk : DnieCheckFail;
}

void dnie_auth_read(
    DnieTransport* poller,
    DnieSm* sm,
    const uint8_t* dg1,
    size_t dg1_len,
    DnieAuth* a) {
    dnie_auth_reset(a);
    size_t total;

    // EF.SOD (solo si hay memoria: es opcional, la lectura sigue sin el)
    if(memmgr_heap_get_max_free_block() < DNIE_SOD_MAX + 2048 ||
       memmgr_get_free_heap() < DNIE_SOD_MAX + 16384) {
        FURI_LOG_E(TAG, "Sin memoria para el SOD (libre %u)", memmgr_get_free_heap());
        a->sod_status.result = DnieLdsErrorSink;
        return;
    }
    a->sod_buf = malloc(DNIE_SOD_MAX);
    if(a->sod_buf) {
        MemSink m = {.buf = a->sod_buf, .max = DNIE_SOD_MAX};
        if(dnie_lds_read_file(
               poller, sm, DNIE_FID_SOD, mem_sink, NULL, &m, &total, &a->sod_status) ==
           DnieLdsOk) {
            a->sod_len = m.len;
            // Encoger el buffer a lo que de verdad ocupa el SOD antes de parsearlo
            uint8_t* shrunk = realloc(a->sod_buf, a->sod_len ? a->sod_len : 1);
            if(shrunk) a->sod_buf = shrunk;
            a->sod = malloc(sizeof(DnieSod));
            if(a->sod && !dnie_sod_parse(a->sod_buf, a->sod_len, a->sod)) {
                FURI_LOG_E(TAG, "SOD no parseable (%u B)", a->sod_len);
                free(a->sod);
                a->sod = NULL;
            }
        }
    }
    // Si el SOD fallo por SM, el canal ya no es utilizable: no seguimos
    if(a->sod_status.result == DnieLdsErrorSm || a->sod_status.result == DnieLdsErrorTransport)
        return;

    // DG14
    a->dg14_buf = malloc(DNIE_DG14_MAX);
    // (se encoge igual que el SOD tras leerlo)
    if(a->dg14_buf) {
        MemSink m = {.buf = a->dg14_buf, .max = DNIE_DG14_MAX};
        if(dnie_lds_read_file(
               poller, sm, DNIE_FID_DG14, mem_sink, NULL, &m, &total, &a->dg14_status) ==
           DnieLdsOk) {
            a->dg14_len = m.len;
            uint8_t* shrunk = realloc(a->dg14_buf, a->dg14_len ? a->dg14_len : 1);
            if(shrunk) a->dg14_buf = shrunk;
            a->dg14_ok = dnie_dg14_parse(a->dg14_buf, a->dg14_len, &a->dg14);
        }
    }

    if(!a->sod) return;

    // Resumen para mostrar
    a->sod_parsed = true;
    a->lds_hash = a->sod->lds_hash;
    a->dg_count = a->sod->dg_count;
    for(uint8_t k = 0; k < a->dg_count; k++)
        a->dg_numbers[k] = a->sod->dgs[k].number;
    strlcpy(a->sig_alg, a->sod->sig_alg, sizeof(a->sig_alg));
    a->has_cert = a->sod->has_cert;
    strlcpy(a->ds_subject_cn, a->sod->ds_subject_cn, sizeof(a->ds_subject_cn));
    strlcpy(a->ds_issuer_cn, a->sod->ds_issuer_cn, sizeof(a->ds_issuer_cn));

    // Firma del SOD con la clave del certificado DS (RSA PKCS#1 v1.5 en el DNIe).
    // La exponenciacion RSA reserva unos KB: solo si hay margen.
    uint32_t t0 = furi_get_tick();
    if(memmgr_get_free_heap() > 12288 && memmgr_heap_get_max_free_block() > 4096) {
        dnie_pa_verify_sod(a->sod, &a->pa);
    } else {
        a->pa.result = DniePaNoMemory;
    }
    FURI_LOG_I(
        TAG,
        "Firma SOD: %d (%u bits) en %lu ms",
        a->pa.result,
        a->pa.key_bits,
        furi_get_tick() - t0);

    // Cadena: el certificado DS tiene que estar firmado por una CSCA SPAIN (RSA-4096)
    if(a->sod->has_cert) {
        t0 = furi_get_tick();
        if(memmgr_get_free_heap() > 16384 && memmgr_heap_get_max_free_block() > 6144) {
            // CSCA de otros paises: fichero de assets en la SD
            Storage* storage = furi_record_open(RECORD_STORAGE);
            File* f = storage_file_alloc(storage);
            bool open =
                storage_file_open(f, APP_ASSETS_PATH("csca.bin"), FSAM_READ, FSOM_OPEN_EXISTING);
            dnie_csca_verify_cert(&a->sod->cert, open ? csca_file_read : NULL, f, &a->csca);
            if(open) storage_file_close(f);
            storage_file_free(f);
            furi_record_close(RECORD_STORAGE);
        } else {
            a->csca.result = DnieCscaNoMemory;
        }
        FURI_LOG_I(TAG, "Cadena CSCA: %d en %lu ms", a->csca.result, furi_get_tick() - t0);
    }

    a->dg1 = check_hash(a->sod, 1, dg1, dg1_len);
    if(a->dg14_ok) a->dg14_hash = check_hash(a->sod, 14, a->dg14_buf, a->dg14_len);
    if(!dnie_hash_supported(a->sod->signer_hash)) {
        a->message_digest = DnieCheckUnsupported;
    } else {
        a->message_digest = dnie_sod_check_message_digest(a->sod) ? DnieCheckOk : DnieCheckFail;
    }
}

bool dnie_auth_dg2_hash_start(const DnieAuth* a, DnieHash* h) {
    if(!a->sod || !dnie_sod_dg_hash(a->sod, 2)) return false;
    return dnie_hash_start(h, a->sod->lds_hash);
}

void dnie_auth_dg2_hash_done(DnieAuth* a, DnieHash* h, bool read_ok) {
    uint8_t out[DNIE_HASH_MAX];
    if(!a->sod) return;
    const uint8_t* expected = dnie_sod_dg_hash(a->sod, 2);
    if(!expected) {
        a->dg2 = DnieCheckMissing;
        return;
    }
    if(!dnie_hash_supported(a->sod->lds_hash)) {
        a->dg2 = DnieCheckUnsupported;
        return;
    }
    bool ok = dnie_hash_finish(h, out);
    if(!read_ok || !ok) {
        a->dg2 = DnieCheckNotDone;
        return;
    }
    a->dg2 = memcmp(out, expected, dnie_hash_len(a->sod->lds_hash)) == 0 ? DnieCheckOk :
                                                                           DnieCheckFail;
}

void dnie_auth_compact(DnieAuth* a) {
    free(a->sod_buf);
    a->sod_buf = NULL;
    a->sod_len = 0;
    free(a->sod);
    a->sod = NULL;
    free(a->dg14_buf);
    a->dg14_buf = NULL;
    a->dg14_len = 0;
    // explicit_params apuntaba dentro del buffer de DG14: ya no es valido
    for(uint8_t i = 0; i < a->dg14.key_count; i++) {
        memset(&a->dg14.keys[i].explicit_params, 0, sizeof(DerTlv));
    }
}

bool dnie_auth_all_ok(const DnieAuth* a) {
    return dnie_auth_integrity_ok(a) && a->pa.result == DniePaOk && a->csca.result == DnieCscaOk &&
           a->ca.result == DnieCaOk;
}

bool dnie_auth_data_ok(const DnieAuth* a) {
    return dnie_auth_integrity_ok(a) && a->pa.result == DniePaOk && a->csca.result == DnieCscaOk &&
           a->ca.result != DnieCaFailed;
}

bool dnie_auth_integrity_failed(const DnieAuth* a) {
    return a->dg1 == DnieCheckFail || a->dg2 == DnieCheckFail || a->dg14_hash == DnieCheckFail ||
           a->message_digest == DnieCheckFail;
}

bool dnie_auth_integrity_ok(const DnieAuth* a) {
    return a->sod_parsed && a->dg1 == DnieCheckOk && a->message_digest == DnieCheckOk &&
           !dnie_auth_integrity_failed(a);
}

const char* dnie_check_str(DnieCheck c) {
    switch(c) {
    case DnieCheckOk:
        return "OK";
    case DnieCheckFail:
        return "MISMATCH";
    case DnieCheckMissing:
        return "not included";
    case DnieCheckUnsupported:
        return "unsupported alg.";
    default:
        return "not checked";
    }
}
