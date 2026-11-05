#include "dnie_pa.h"

#include <stdlib.h>
#include <string.h>
#include <mbedtls/bignum.h>

static const uint8_t OID_RSA_ENCRYPTION[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
static const uint8_t OID_RSA_PREFIX[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01};

// DigestInfo DER (sin el hash) de cada algoritmo, con parametros NULL (forma habitual)
static const uint8_t DI_SHA1[] =
    {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1A, 0x05, 0x00, 0x04, 0x14};
static const uint8_t DI_SHA224[] = {
    0x30,
    0x2D,
    0x30,
    0x0D,
    0x06,
    0x09,
    0x60,
    0x86,
    0x48,
    0x01,
    0x65,
    0x03,
    0x04,
    0x02,
    0x04,
    0x05,
    0x00,
    0x04,
    0x1C};
static const uint8_t DI_SHA256[] = {
    0x30,
    0x31,
    0x30,
    0x0D,
    0x06,
    0x09,
    0x60,
    0x86,
    0x48,
    0x01,
    0x65,
    0x03,
    0x04,
    0x02,
    0x01,
    0x05,
    0x00,
    0x04,
    0x20};

static const uint8_t* digest_info(DnieHashAlg alg, size_t* len) {
    switch(alg) {
    case DnieHashSha1:
        *len = sizeof(DI_SHA1);
        return DI_SHA1;
    case DnieHashSha224:
        *len = sizeof(DI_SHA224);
        return DI_SHA224;
    case DnieHashSha256:
        *len = sizeof(DI_SHA256);
        return DI_SHA256;
    default:
        return NULL;
    }
}

// Comprueba el DigestInfo de EM, aceptando tambien la variante sin NULL (30 xx 30 yy 06 OID 04 ..)
static bool
    digest_info_matches(DnieHashAlg alg, const uint8_t* di, size_t len, const uint8_t* hash) {
    size_t ref_len;
    const uint8_t* ref = digest_info(alg, &ref_len);
    size_t hl = dnie_hash_len(alg);
    if(!ref) return false;
    if(len == ref_len + hl && memcmp(di, ref, ref_len) == 0 && memcmp(di + ref_len, hash, hl) == 0)
        return true;
    // Sin "05 00": longitudes 2 menos en las dos SEQUENCE y el OID igual
    size_t oid_len = ref[5];
    size_t alt_len = ref_len - 2;
    if(len != alt_len + hl) return false;
    if(di[0] != 0x30 || di[1] != ref[1] - 2 || di[2] != 0x30 || di[3] != ref[3] - 2) return false;
    if(memcmp(di + 4, ref + 4, 2 + oid_len) != 0) return false;
    if(di[6 + oid_len] != 0x04 || di[7 + oid_len] != hl) return false;
    return memcmp(di + 8 + oid_len, hash, hl) == 0;
}

DniePaResult dnie_pa_rsa_pkcs1_verify(
    const uint8_t* n,
    size_t n_len,
    const uint8_t* e,
    size_t e_len,
    DnieHashAlg alg,
    const uint8_t* hash,
    const uint8_t* sig,
    size_t sig_len) {
    // Quitar ceros a la izquierda del modulo (INTEGER positivo)
    while(n_len > 0 && n[0] == 0) {
        n++;
        n_len--;
    }
    if(n_len < 64 || n_len > 512 || sig_len > n_len + 1) return DniePaFormat;

    mbedtls_mpi N, E, S, M;
    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&E);
    mbedtls_mpi_init(&S);
    mbedtls_mpi_init(&M);
    uint8_t* em = malloc(n_len);
    DniePaResult r = DniePaBadSignature;

    do {
        if(!em || mbedtls_mpi_read_binary(&N, n, n_len) != 0 ||
           mbedtls_mpi_read_binary(&E, e, e_len) != 0 ||
           mbedtls_mpi_read_binary(&S, sig, sig_len) != 0) {
            r = DniePaFormat;
            break;
        }
        if(mbedtls_mpi_cmp_mpi(&S, &N) >= 0) break; // firma fuera de rango
        // M = S^E mod N (operacion publica: solo usa la clave publica)
        if(mbedtls_mpi_exp_mod(&M, &S, &E, &N, NULL) != 0) {
            r = DniePaFormat;
            break;
        }
        if(mbedtls_mpi_write_binary(&M, em, n_len) != 0) break;

        // EM = 00 01 FF..FF 00 DigestInfo  (al menos 8 bytes FF)
        if(em[0] != 0x00 || em[1] != 0x01) break;
        size_t i = 2;
        while(i < n_len && em[i] == 0xFF)
            i++;
        if(i - 2 < 8 || i >= n_len || em[i] != 0x00) break;
        i++;
        if(digest_info_matches(alg, em + i, n_len - i, hash)) r = DniePaOk;
    } while(0);

    if(em) {
        memset(em, 0, n_len);
        free(em);
    }
    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&E);
    mbedtls_mpi_free(&S);
    mbedtls_mpi_free(&M);
    return r;
}

// ---- RSASSA-PSS ----

static const uint8_t OID_RSA_PSS[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0A};
static const uint8_t OID_MGF1[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x08};

// Hash de un AlgorithmIdentifier { OID, NULL? }
static DnieHashAlg alg_id_hash(const DerTlv* seq) {
    DerIt a = der_enter(seq);
    DerTlv oid;
    if(!der_expect(&a, 0x06, &oid)) return DnieHashUnknown;
    return dnie_hash_from_oid(&oid);
}

bool dnie_pa_rsa_sig_alg(const DerTlv* alg_seq, DnieHashAlg* hash, bool* pss, int* salt_len) {
    *pss = false;
    *salt_len = -1;
    *hash = DnieHashUnknown;
    if(alg_seq->tag != 0x30) return false;
    DerIt a = der_enter(alg_seq);
    DerTlv oid, params;
    if(!der_expect(&a, 0x06, &oid)) return false;
    if(der_oid_is(&oid, OID_RSA_PSS, sizeof(OID_RSA_PSS))) {
        // RSASSA-PSS-params: [0] hash, [1] MGF, [2] sal, [3] trailer. Por defecto SHA-1 y 20.
        *pss = true;
        *hash = DnieHashSha1;
        *salt_len = 20;
        DnieHashAlg mgf_hash = DnieHashSha1;
        if(der_next(&a, &params) && params.tag == 0x30) {
            DerIt p = der_enter(&params);
            DerTlv f;
            while(der_next(&p, &f)) {
                DerIt in = der_enter(&f);
                DerTlv v;
                if(!der_next(&in, &v)) return false;
                if(f.tag == 0xA0) {
                    *hash = alg_id_hash(&v);
                } else if(f.tag == 0xA1) {
                    // MGF1 { id-mgf1, AlgorithmIdentifier hash }
                    DerIt m = der_enter(&v);
                    DerTlv moid, mh;
                    if(!der_expect(&m, 0x06, &moid) ||
                       !der_oid_is(&moid, OID_MGF1, sizeof(OID_MGF1)) ||
                       !der_expect(&m, 0x30, &mh))
                        return false;
                    mgf_hash = alg_id_hash(&mh);
                } else if(f.tag == 0xA2) {
                    uint32_t sl;
                    if(!der_uint(&v, &sl) || sl > 512) return false;
                    *salt_len = (int)sl;
                }
            }
        }
        return *hash != DnieHashUnknown && mgf_hash == *hash;
    }
    if(oid.len == sizeof(OID_RSA_PREFIX) + 1 &&
       memcmp(oid.val, OID_RSA_PREFIX, sizeof(OID_RSA_PREFIX)) == 0) {
        switch(oid.val[sizeof(OID_RSA_PREFIX)]) {
        case 5:
            *hash = DnieHashSha1;
            return true;
        case 11:
            *hash = DnieHashSha256;
            return true;
        case 12:
            *hash = DnieHashSha384;
            return true;
        case 13:
            *hash = DnieHashSha512;
            return true;
        case 14:
            *hash = DnieHashSha224;
            return true;
        default:
            return false;
        }
    }
    return false;
}

DniePaResult dnie_pa_rsa_pss_verify(
    const uint8_t* n,
    size_t n_len,
    const uint8_t* e,
    size_t e_len,
    DnieHashAlg alg,
    const uint8_t* hash,
    const uint8_t* sig,
    size_t sig_len,
    int salt_len) {
    while(n_len > 0 && n[0] == 0) {
        n++;
        n_len--;
    }
    size_t hl = dnie_hash_len(alg);
    if(n_len < 64 || n_len > 512 || sig_len > n_len + 1 || !dnie_hash_supported(alg))
        return DniePaFormat;

    // Bits del modulo: emBits = modBits - 1
    int top = 8;
    while(top > 0 && !(n[0] & (1 << (top - 1))))
        top--;
    size_t mod_bits = (n_len - 1) * 8 + top;
    size_t em_bits = mod_bits - 1;
    size_t em_len = (em_bits + 7) / 8;

    mbedtls_mpi N, E, S, M;
    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&E);
    mbedtls_mpi_init(&S);
    mbedtls_mpi_init(&M);
    uint8_t* buf = malloc(n_len * 2);
    DniePaResult r = DniePaBadSignature;

    do {
        if(!buf || mbedtls_mpi_read_binary(&N, n, n_len) != 0 ||
           mbedtls_mpi_read_binary(&E, e, e_len) != 0 ||
           mbedtls_mpi_read_binary(&S, sig, sig_len) != 0) {
            r = DniePaFormat;
            break;
        }
        if(mbedtls_mpi_cmp_mpi(&S, &N) >= 0) break;
        if(mbedtls_mpi_exp_mod(&M, &S, &E, &N, NULL) != 0) {
            r = DniePaFormat;
            break;
        }
        uint8_t* full = buf;
        if(mbedtls_mpi_write_binary(&M, full, n_len) != 0) break;
        // Si emLen < k el primer byte tiene que ser 0
        uint8_t* em = full + (n_len - em_len);
        if(n_len > em_len && full[0] != 0) break;
        if(em_len < hl + 2 || em[em_len - 1] != 0xBC) break;

        size_t db_len = em_len - hl - 1;
        const uint8_t* h = em + db_len;
        uint8_t* db = buf + n_len; // masked DB -> DB
        memcpy(db, em, db_len);
        // MGF1(H): DB ^= Hash(H || contador)
        uint8_t t[DNIE_HASH_MAX];
        for(uint32_t c = 0, off = 0; off < db_len; c++) {
            uint8_t cnt[4] = {c >> 24, c >> 16, c >> 8, c};
            DnieHash hs;
            dnie_hash_start(&hs, alg);
            dnie_hash_update(&hs, h, hl);
            dnie_hash_update(&hs, cnt, 4);
            dnie_hash_finish(&hs, t);
            for(size_t k = 0; k < hl && off < db_len; k++)
                db[off++] ^= t[k];
        }
        db[0] &= 0xFF >> (8 * em_len - em_bits);
        // DB = 00..00 || 01 || sal
        size_t i = 0;
        while(i < db_len && db[i] == 0)
            i++;
        if(i >= db_len || db[i] != 0x01) break;
        i++;
        size_t sl = db_len - i;
        if(salt_len >= 0 && sl != (size_t)salt_len) break;
        // H' = Hash(00*8 || mHash || sal)
        static const uint8_t zeros[8] = {0};
        DnieHash hs;
        dnie_hash_start(&hs, alg);
        dnie_hash_update(&hs, zeros, 8);
        dnie_hash_update(&hs, hash, hl);
        dnie_hash_update(&hs, db + i, sl);
        dnie_hash_finish(&hs, t);
        if(memcmp(t, h, hl) == 0) r = DniePaOk;
    } while(0);

    if(buf) {
        memset(buf, 0, n_len * 2);
        free(buf);
    }
    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&E);
    mbedtls_mpi_free(&S);
    mbedtls_mpi_free(&M);
    return r;
}

// Saca n y e de la clave publica RSA del certificado
static bool cert_rsa_key(const DerTlv* cert, DerTlv* n, DerTlv* e) {
    DerIt c = der_enter(cert);
    DerTlv tbs, f, spki, alg, oid, bits;
    if(!der_expect(&c, 0x30, &tbs)) return false;
    DerIt t = der_enter(&tbs);
    if(!der_next(&t, &f)) return false;
    if(f.tag == 0xA0 && !der_next(&t, &f)) return false; // version -> serial
    // sigAlg, issuer, validity, subject, SPKI
    for(int k = 0; k < 4; k++)
        if(!der_next(&t, &f)) return false;
    if(!der_expect(&t, 0x30, &spki)) return false;
    DerIt s = der_enter(&spki);
    if(!der_expect(&s, 0x30, &alg) || !der_expect(&s, 0x03, &bits)) return false;
    DerIt a = der_enter(&alg);
    if(!der_expect(&a, 0x06, &oid) ||
       !der_oid_is(&oid, OID_RSA_ENCRYPTION, sizeof(OID_RSA_ENCRYPTION)))
        return false;
    if(bits.len < 2 || bits.val[0] != 0) return false;
    DerIt k = der_it(bits.val + 1, bits.len - 1);
    DerTlv seq;
    if(!der_expect(&k, 0x30, &seq)) return false;
    DerIt ks = der_enter(&seq);
    return der_expect(&ks, 0x02, n) && der_expect(&ks, 0x02, e);
}

// Hash para la firma: del OID de firma (sha256WithRSA...) o, si es rsaEncryption a secas,
// del digestAlgorithm del SignerInfo
static DnieHashAlg signature_hash(const DnieSod* sod, bool* is_rsa) {
    *is_rsa = false;
    const DerTlv* oid = &sod->sig_alg_oid;
    if(oid->tag != 0x06) return DnieHashUnknown;
    if(der_oid_is(oid, OID_RSA_ENCRYPTION, sizeof(OID_RSA_ENCRYPTION))) {
        *is_rsa = true;
        return sod->signer_hash;
    }
    if(oid->len == sizeof(OID_RSA_PREFIX) + 1 &&
       memcmp(oid->val, OID_RSA_PREFIX, sizeof(OID_RSA_PREFIX)) == 0) {
        *is_rsa = true;
        switch(oid->val[sizeof(OID_RSA_PREFIX)]) {
        case 5:
            return DnieHashSha1;
        case 11:
            return DnieHashSha256;
        case 14:
            return DnieHashSha224;
        default:
            return DnieHashUnknown; // PSS (10), SHA-384/512...
        }
    }
    return DnieHashUnknown;
}

DniePaResult dnie_pa_verify_sod(const DnieSod* sod, DniePaInfo* info) {
    memset(info, 0, sizeof(*info));
    if(!sod->has_cert) return info->result = DniePaNoCert;
    if(!sod->signed_attrs.raw || sod->signed_attrs.raw_len < 2) return info->result = DniePaFormat;

    bool is_rsa;
    DnieHashAlg alg = signature_hash(sod, &is_rsa);
    bool pss = false;
    int salt = -1;
    if(!is_rsa || alg == DnieHashUnknown) {
        // RSASSA-PSS (pasaportes de otros paises)
        DnieHashAlg h;
        is_rsa = false;
        if(dnie_pa_rsa_sig_alg(&sod->sig_alg_seq, &h, &pss, &salt) && pss) {
            alg = h;
            is_rsa = true;
        }
    }
    info->hash = alg;
    if(!is_rsa || !dnie_hash_supported(alg)) return info->result = DniePaUnsupported;

    DerTlv n, e;
    if(!cert_rsa_key(&sod->cert, &n, &e)) return info->result = DniePaUnsupported;
    size_t nl = n.len;
    const uint8_t* nv = n.val;
    while(nl > 0 && *nv == 0) {
        nv++;
        nl--;
    }
    info->key_bits = (uint16_t)(nl * 8);

    // Lo firmado son los signedAttrs con su etiqueta [0] cambiada por SET (0x31)
    uint8_t h[DNIE_HASH_MAX];
    DnieHash hs;
    const uint8_t set_tag = 0x31;
    if(!dnie_hash_start(&hs, alg)) return info->result = DniePaUnsupported;
    dnie_hash_update(&hs, &set_tag, 1);
    dnie_hash_update(&hs, sod->signed_attrs.raw + 1, sod->signed_attrs.raw_len - 1);
    if(!dnie_hash_finish(&hs, h)) return info->result = DniePaFormat;

    info->result =
        pss ?
            dnie_pa_rsa_pss_verify(
                n.val, n.len, e.val, e.len, alg, h, sod->signature.val, sod->signature.len, salt) :
            dnie_pa_rsa_pkcs1_verify(
                n.val, n.len, e.val, e.len, alg, h, sod->signature.val, sod->signature.len);
    return info->result;
}

const char* dnie_pa_result_str(DniePaResult r) {
    switch(r) {
    case DniePaOk:
        return "OK";
    case DniePaBadSignature:
        return "INVALID";
    case DniePaNoCert:
        return "no certificate";
    case DniePaUnsupported:
        return "unsupported alg.";
    case DniePaFormat:
        return "malformed";
    case DniePaNoMemory:
        return "out of memory";
    default:
        return "not checked";
    }
}
