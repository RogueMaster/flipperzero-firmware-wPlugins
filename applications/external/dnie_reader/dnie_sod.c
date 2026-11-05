#include "dnie_sod.h"

#include <stdio.h>
#include <string.h>

// --- OIDs ---
static const uint8_t OID_SIGNED_DATA[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x02};
static const uint8_t OID_LDS_SO[] = {0x67, 0x81, 0x08, 0x01, 0x01, 0x01};
static const uint8_t OID_MESSAGE_DIGEST[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x04};
static const uint8_t OID_COMMON_NAME[] = {0x55, 0x04, 0x03};

static const uint8_t OID_SHA1[] = {0x2B, 0x0E, 0x03, 0x02, 0x1A};
static const uint8_t OID_SHA2_PREFIX[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02};

DnieHashAlg dnie_hash_from_oid(const DerTlv* oid) {
    if(der_oid_is(oid, OID_SHA1, sizeof(OID_SHA1))) return DnieHashSha1;
    if(oid->tag == 0x06 && oid->len == sizeof(OID_SHA2_PREFIX) + 1 &&
       memcmp(oid->val, OID_SHA2_PREFIX, sizeof(OID_SHA2_PREFIX)) == 0) {
        switch(oid->val[sizeof(OID_SHA2_PREFIX)]) {
        case 1:
            return DnieHashSha256;
        case 2:
            return DnieHashSha384;
        case 3:
            return DnieHashSha512;
        case 4:
            return DnieHashSha224;
        }
    }
    return DnieHashUnknown;
}

// Nombre legible de los algoritmos de firma habituales en pasaportes/DNIs
static void sig_alg_name(const DerTlv* oid, char* out, size_t size) {
    static const struct {
        uint8_t enc[9];
        uint8_t len;
        const char* name;
    } names[] = {
        {{0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x01}, 7, "ECDSA-SHA1"},
        {{0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x01}, 8, "ECDSA-SHA224"},
        {{0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02}, 8, "ECDSA-SHA256"},
        {{0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x03}, 8, "ECDSA-SHA384"},
        {{0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x04}, 8, "ECDSA-SHA512"},
        {{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01}, 9, "RSA (PKCS#1)"},
        {{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x05}, 9, "RSA-SHA1"},
        {{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0A}, 9, "RSA-PSS"},
        {{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B}, 9, "RSA-SHA256"},
        {{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0C}, 9, "RSA-SHA384"},
        {{0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0D}, 9, "RSA-SHA512"},
    };
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if(der_oid_is(oid, names[i].enc, names[i].len)) {
            snprintf(out, size, "%s", names[i].name);
            return;
        }
    }
    der_oid_str(oid, out, size);
}

// AlgorithmIdentifier: SEQUENCE { OID, params opcionales } -> TLV del OID
static bool alg_oid(const DerTlv* seq, DerTlv* oid) {
    if(seq->tag != 0x30) return false;
    DerIt it = der_enter(seq);
    return der_expect(&it, 0x06, oid);
}

// Busca el CN dentro de un Name (SEQUENCE OF SET OF {OID, valor})
static void name_cn(const DerTlv* name, char* out, size_t size) {
    out[0] = '\0';
    DerIt rdns = der_enter(name);
    DerTlv rdn;
    while(der_next(&rdns, &rdn)) {
        DerIt atvs = der_enter(&rdn);
        DerTlv atv;
        while(der_next(&atvs, &atv)) {
            DerIt a = der_enter(&atv);
            DerTlv oid, val;
            if(der_expect(&a, 0x06, &oid) && der_next(&a, &val) &&
               der_oid_is(&oid, OID_COMMON_NAME, sizeof(OID_COMMON_NAME))) {
                size_t n = val.len < size - 1 ? val.len : size - 1;
                memcpy(out, val.val, n);
                out[n] = '\0';
            }
        }
    }
}

static bool parse_lds_so(const DerTlv* econtent, DnieSod* out) {
    DerIt it = der_enter(econtent);
    DerTlv so, version, alg, list, oid;
    if(!der_expect(&it, 0x30, &so)) return false;
    DerIt s = der_enter(&so);
    if(!der_expect(&s, 0x02, &version)) return false;
    if(!der_expect(&s, 0x30, &alg) || !alg_oid(&alg, &oid)) return false;
    out->lds_hash = dnie_hash_from_oid(&oid);
    if(!der_expect(&s, 0x30, &list)) return false;

    DerIt l = der_enter(&list);
    DerTlv entry;
    while(der_next(&l, &entry) && out->dg_count < DNIE_SOD_MAX_DG) {
        DerIt e = der_enter(&entry);
        DerTlv num, hash;
        uint32_t n;
        if(!der_expect(&e, 0x02, &num) || !der_uint(&num, &n)) return false;
        if(!der_expect(&e, 0x04, &hash) || hash.len > sizeof(out->dgs[0].hash)) return false;
        DnieSodDg* dg = &out->dgs[out->dg_count++];
        dg->number = (uint8_t)n;
        memcpy(dg->hash, hash.val, hash.len);
    }
    return out->dg_count > 0;
}

static void parse_certificate(const DerTlv* cert, DnieSod* out) {
    // Certificate { TBSCertificate { [0] version, serial, sigAlg, issuer, validity, subject, ... } }
    DerIt c = der_enter(cert);
    DerTlv tbs;
    if(!der_expect(&c, 0x30, &tbs)) return;
    DerIt t = der_enter(&tbs);
    DerTlv f;
    if(!der_next(&t, &f)) return;
    if(f.tag == 0xA0 && !der_next(&t, &f)) return; // version -> serial
    DerTlv sig, issuer, validity, subject;
    if(!der_next(&t, &sig) || !der_next(&t, &issuer) || !der_next(&t, &validity) ||
       !der_next(&t, &subject))
        return;
    name_cn(&issuer, out->ds_issuer_cn, sizeof(out->ds_issuer_cn));
    name_cn(&subject, out->ds_subject_cn, sizeof(out->ds_subject_cn));
}

static bool parse_signer_info(const DerTlv* si, DnieSod* out) {
    DerIt s = der_enter(si);
    DerTlv version, sid, dalg, f, oid;
    if(!der_expect(&s, 0x02, &version) || !der_next(&s, &sid)) return false;
    if(!der_expect(&s, 0x30, &dalg) || !alg_oid(&dalg, &oid)) return false;
    out->signer_hash = dnie_hash_from_oid(&oid);
    if(!der_next(&s, &f)) return false;

    if(f.tag == 0xA0) { // signedAttrs
        out->signed_attrs = f;
        DerIt attrs = der_enter(&f);
        DerTlv attr;
        while(der_next(&attrs, &attr)) {
            DerIt a = der_enter(&attr);
            DerTlv aoid, values, v;
            if(der_expect(&a, 0x06, &aoid) && der_expect(&a, 0x31, &values) &&
               der_oid_is(&aoid, OID_MESSAGE_DIGEST, sizeof(OID_MESSAGE_DIGEST))) {
                DerIt vs = der_enter(&values);
                if(der_expect(&vs, 0x04, &v) && v.len <= sizeof(out->message_digest)) {
                    memcpy(out->message_digest, v.val, v.len);
                    out->message_digest_len = v.len;
                    out->has_message_digest = true;
                }
            }
        }
        if(!der_next(&s, &f)) return false;
    }
    if(!alg_oid(&f, &oid)) return false; // signatureAlgorithm
    out->sig_alg_oid = oid;
    out->sig_alg_seq = f; // con los parametros (RSA-PSS)
    sig_alg_name(&oid, out->sig_alg, sizeof(out->sig_alg));
    return der_expect(&s, 0x04, &out->signature);
}

bool dnie_sod_parse(const uint8_t* data, size_t len, DnieSod* out) {
    memset(out, 0, sizeof(*out));
    DerIt it = der_it(data, len);
    DerTlv t77, ci, oid, a0, sd, f;
    if(!der_expect(&it, 0x77, &t77)) return false;
    DerIt i = der_enter(&t77);
    if(!der_expect(&i, 0x30, &ci)) return false;
    DerIt c = der_enter(&ci);
    if(!der_expect(&c, 0x06, &oid) || !der_oid_is(&oid, OID_SIGNED_DATA, sizeof(OID_SIGNED_DATA)))
        return false;
    if(!der_expect(&c, 0xA0, &a0)) return false;
    DerIt a = der_enter(&a0);
    if(!der_expect(&a, 0x30, &sd)) return false;

    DerIt s = der_enter(&sd);
    DerTlv version, digest_algs, encap;
    if(!der_expect(&s, 0x02, &version) || !der_expect(&s, 0x31, &digest_algs)) return false;
    if(!der_expect(&s, 0x30, &encap)) return false;

    // encapContentInfo { id-icao-ldsSecurityObject, [0] { OCTET STRING } }
    DerIt e = der_enter(&encap);
    DerTlv ctype, ex, octets;
    if(!der_expect(&e, 0x06, &ctype) || !der_oid_is(&ctype, OID_LDS_SO, sizeof(OID_LDS_SO)))
        return false;
    if(!der_expect(&e, 0xA0, &ex)) return false;
    DerIt x = der_enter(&ex);
    if(!der_expect(&x, 0x04, &octets)) return false;
    out->econtent = octets;
    if(!parse_lds_so(&octets, out)) return false;

    // [0] certificates, [1] crls (opcionales), signerInfos
    while(der_next(&s, &f)) {
        if(f.tag == 0xA0) {
            DerIt certs = der_enter(&f);
            if(der_expect(&certs, 0x30, &out->cert)) {
                out->has_cert = true;
                parse_certificate(&out->cert, out);
            }
        } else if(f.tag == 0x31) {
            DerIt sis = der_enter(&f);
            DerTlv si;
            if(!der_expect(&sis, 0x30, &si)) return false;
            return parse_signer_info(&si, out);
        }
    }
    return false;
}

const uint8_t* dnie_sod_dg_hash(const DnieSod* sod, uint8_t dg) {
    for(uint8_t i = 0; i < sod->dg_count; i++) {
        if(sod->dgs[i].number == dg) return sod->dgs[i].hash;
    }
    return NULL;
}

bool dnie_sod_check_message_digest(const DnieSod* sod) {
    if(!sod->has_message_digest || !dnie_hash_supported(sod->signer_hash)) return false;
    size_t n = dnie_hash_len(sod->signer_hash);
    if(sod->message_digest_len != n) return false;
    uint8_t h[DNIE_HASH_MAX];
    return dnie_hash(sod->signer_hash, sod->econtent.val, sod->econtent.len, h) &&
           memcmp(h, sod->message_digest, n) == 0;
}
