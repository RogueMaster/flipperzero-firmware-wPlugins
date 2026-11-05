#include "dnie_csca.h"
#include "dnie_pa.h"

#include <stdlib.h>
#include <string.h>

#include "dnie_csca_keys.inc"

#define CSCA_COUNT (sizeof(DNIE_CSCA_KEYS) / sizeof(DNIE_CSCA_KEYS[0]))
#define REC_MAX    1024

typedef struct {
    DnieHashAlg hash;
    bool pss;
    int salt;
    uint8_t h[DNIE_HASH_MAX];
    const uint8_t* sig;
    size_t sig_len;
} Sig;

static DnieCscaResult
    try_key(const uint8_t* n, size_t nl, const uint8_t* e, size_t el, const Sig* s) {
    DniePaResult r =
        s->pss ? dnie_pa_rsa_pss_verify(n, nl, e, el, s->hash, s->h, s->sig, s->sig_len, s->salt) :
                 dnie_pa_rsa_pkcs1_verify(n, nl, e, el, s->hash, s->h, s->sig, s->sig_len);
    if(r == DniePaOk) return DnieCscaOk;
    if(r == DniePaBadSignature) return DnieCscaBadSignature;
    return DnieCscaFormat;
}

// Recorre csca.bin: u16 len | tipo | 'CC' | len_nombre | nombre | u16 len_subj | subj
//                   | [RSA: u16 len_n | n | len_e | e]
static DnieCscaResult scan_file(
    DnieCscaRead read,
    void* ctx,
    const DerTlv* issuer,
    const Sig* s,
    bool any_subject,
    DnieCscaInfo* info) {
    uint8_t* rec = malloc(REC_MAX);
    if(!rec) return DnieCscaNoMemory;
    DnieCscaResult res = DnieCscaUnknownIssuer;
    uint8_t hdr[2];
    while(read(ctx, hdr, 2) == 2) {
        size_t len = (size_t)hdr[0] << 8 | hdr[1];
        if(len > REC_MAX) break; // fichero corrupto
        if(read(ctx, rec, len) != len) break;
        size_t p = 0;
        if(len < 4) break;
        uint8_t type = rec[p++];
        p += 2; // pais
        size_t nm = rec[p++];
        const uint8_t* name = rec + p;
        p += nm;
        if(p + 2 > len) break;
        size_t sl = (size_t)rec[p] << 8 | rec[p + 1];
        p += 2;
        const uint8_t* subj = rec + p;
        p += sl;
        if(p > len) break;
        bool same = issuer->raw_len == sl && memcmp(issuer->raw, subj, sl) == 0;
        if(!same && !any_subject) continue;
        if(same && type != 1) {
            // Emisor conocido pero con clave ECDSA: aun no lo verificamos
            res = DnieCscaUnsupported;
            size_t c = nm < sizeof(info->csca_name) - 1 ? nm : sizeof(info->csca_name) - 1;
            memcpy(info->csca_name, name, c);
            info->csca_name[c] = '\0';
            continue;
        }
        if(type != 1 || p + 2 > len) continue;
        size_t nl = (size_t)rec[p] << 8 | rec[p + 1];
        p += 2;
        const uint8_t* n = rec + p;
        p += nl;
        if(p + 1 > len) continue;
        size_t el = rec[p++];
        if(p + el > len) continue;
        DnieCscaResult r = try_key(n, nl, rec + p, el, s);
        if(r == DnieCscaOk || (same && res != DnieCscaOk)) {
            size_t c = nm < sizeof(info->csca_name) - 1 ? nm : sizeof(info->csca_name) - 1;
            memcpy(info->csca_name, name, c);
            info->csca_name[c] = '\0';
        }
        if(r == DnieCscaOk) {
            res = r;
            break;
        }
        if(same) res = r;
    }
    memset(rec, 0, REC_MAX);
    free(rec);
    // Otros paises: no tenemos TODAS sus claves (caducadas, enlaces...), asi que una firma
    // que no cuadra no demuestra falsificacion. Solo con Espana se dice FALSA.
    if(res == DnieCscaBadSignature) res = DnieCscaKeyNotFound;
    return res;
}

DnieCscaResult
    dnie_csca_verify_cert(const DerTlv* cert, DnieCscaRead read, void* ctx, DnieCscaInfo* info) {
    memset(info, 0, sizeof(*info));
    DerIt c = der_enter(cert);
    DerTlv tbs, alg, sig, f, issuer;
    if(!der_expect(&c, 0x30, &tbs) || !der_expect(&c, 0x30, &alg) || !der_expect(&c, 0x03, &sig) ||
       sig.len < 2 || sig.val[0] != 0)
        return info->result = DnieCscaFormat;

    DerIt t = der_enter(&tbs);
    if(!der_next(&t, &f)) return info->result = DnieCscaFormat;
    if(f.tag == 0xA0 && !der_next(&t, &f)) return info->result = DnieCscaFormat; // -> serial
    if(!der_next(&t, &f) || !der_expect(&t, 0x30, &issuer)) return info->result = DnieCscaFormat;

    Sig s = {.sig = sig.val + 1, .sig_len = sig.len - 1};
    bool rsa = dnie_pa_rsa_sig_alg(&alg, &s.hash, &s.pss, &s.salt);
    if(!rsa || !dnie_hash_supported(s.hash)) {
        info->result = DnieCscaUnsupported; // ECDSA o SHA-384/512
        return info->result;
    }
    DnieHash hs;
    if(!dnie_hash_start(&hs, s.hash)) return info->result = DnieCscaUnsupported;
    dnie_hash_update(&hs, tbs.raw, tbs.raw_len);
    if(!dnie_hash_finish(&hs, s.h)) return info->result = DnieCscaFormat;

    // 1. CSCA SPAIN (dentro de la app), primero por nombre exacto
    DnieCscaResult res = DnieCscaUnknownIssuer;
    for(size_t i = 0; i < CSCA_COUNT; i++) {
        const DnieCscaKey* k = &DNIE_CSCA_KEYS[i];
        if(issuer.raw_len != k->subject_len || memcmp(issuer.raw, k->subject, k->subject_len) != 0)
            continue;
        strlcpy(info->csca_name, k->name, sizeof(info->csca_name));
        uint8_t e[4] = {k->e >> 24, k->e >> 16, k->e >> 8, k->e};
        res = try_key(k->n, k->n_len, e, sizeof(e), &s);
        if(res == DnieCscaOk) return info->result = res;
    }
    if(res != DnieCscaUnknownIssuer) return info->result = res;

    // 2. Resto de paises (fichero de la SD), por nombre exacto
    if(read) {
        res = scan_file(read, ctx, &issuer, &s, false, info);
        if(res != DnieCscaUnknownIssuer) return info->result = res;
    }

    // 3. Nombre codificado distinto: probar las claves de Espana de todas formas
    for(size_t i = 0; i < CSCA_COUNT; i++) {
        const DnieCscaKey* k = &DNIE_CSCA_KEYS[i];
        uint8_t e[4] = {k->e >> 24, k->e >> 16, k->e >> 8, k->e};
        if(try_key(k->n, k->n_len, e, sizeof(e), &s) == DnieCscaOk) {
            strlcpy(info->csca_name, k->name, sizeof(info->csca_name));
            return info->result = DnieCscaOk;
        }
    }
    return info->result = DnieCscaUnknownIssuer;
}

const char* dnie_csca_result_str(DnieCscaResult r) {
    switch(r) {
    case DnieCscaOk:
        return "OK";
    case DnieCscaBadSignature:
        return "FAKE";
    case DnieCscaUnknownIssuer:
        return "unknown issuer";
    case DnieCscaUnsupported:
        return "unsupported alg.";
    case DnieCscaFormat:
        return "malformed";
    case DnieCscaNoMemory:
        return "out of memory";
    case DnieCscaKeyNotFound:
        return "key not found";
    default:
        return "not checked";
    }
}
