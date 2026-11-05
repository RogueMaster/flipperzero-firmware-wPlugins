#include "dnie_dg14.h"

#include <string.h>

// bsi-de protocols: 0.4.0.127.0.7.2.2.x
static const uint8_t BSI_PROTO[] = {0x04, 0x00, 0x7F, 0x00, 0x07, 0x02, 0x02};
#define BSI_PK   1
#define BSI_TA   2
#define BSI_CA   3
#define BSI_PACE 4
// Active Authentication: 2.23.136.1.1.5
static const uint8_t OID_AA[] = {0x67, 0x81, 0x08, 0x01, 0x01, 0x05};
// Curvas por nombre
static const uint8_t OID_BP256R1[] = {0x2B, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01, 0x07};
static const uint8_t OID_P256[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};
// Primo de brainpoolP256r1, para reconocerla si viene con parametros explicitos
static const uint8_t BP256_P_PREFIX[] = {0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC};

// Devuelve el arco 'proto' (1..4) si el OID cuelga de bsi-de protocols, y los arcos siguientes
static int bsi_proto(const DerTlv* oid, uint8_t* a, uint8_t* b) {
    if(oid->tag != 0x06 || oid->len < sizeof(BSI_PROTO) + 1) return 0;
    if(memcmp(oid->val, BSI_PROTO, sizeof(BSI_PROTO)) != 0) return 0;
    size_t extra = oid->len - sizeof(BSI_PROTO);
    *a = extra > 1 ? oid->val[sizeof(BSI_PROTO) + 1] : 0;
    *b = extra > 2 ? oid->val[sizeof(BSI_PROTO) + 2] : 0;
    return oid->val[sizeof(BSI_PROTO)];
}

// Reconoce brainpoolP256r1 en parametros explicitos (ECParameters: version, fieldID{oid, p}, ...)
static DnieCurve explicit_curve(const DerTlv* params) {
    DerIt it = der_enter(params);
    DerTlv version, field;
    if(!der_expect(&it, 0x02, &version) || !der_expect(&it, 0x30, &field))
        return DnieCurveExplicit;
    DerIt f = der_enter(&field);
    DerTlv ftype, prime;
    if(!der_expect(&f, 0x06, &ftype) || !der_expect(&f, 0x02, &prime)) return DnieCurveExplicit;
    const uint8_t* p = prime.val;
    size_t n = prime.len;
    if(n > 0 && p[0] == 0) { // INTEGER positivo con 00 delante
        p++;
        n--;
    }
    if(n == 32 && memcmp(p, BP256_P_PREFIX, sizeof(BP256_P_PREFIX)) == 0)
        return DnieCurveBrainpoolP256r1;
    return DnieCurveExplicit;
}

static void parse_public_key(const DerTlv* spki, DnieCaKey* key) {
    // SubjectPublicKeyInfo { AlgorithmIdentifier { id-ecPublicKey, params }, BIT STRING }
    DerIt s = der_enter(spki);
    DerTlv alg, bits;
    if(!der_expect(&s, 0x30, &alg) || !der_expect(&s, 0x03, &bits)) return;
    DerIt a = der_enter(&alg);
    DerTlv aoid, params;
    if(der_expect(&a, 0x06, &aoid) && der_next(&a, &params)) {
        uint32_t id;
        if(params.tag == 0x06) {
            if(der_oid_is(&params, OID_BP256R1, sizeof(OID_BP256R1)))
                key->curve = DnieCurveBrainpoolP256r1;
            else if(der_oid_is(&params, OID_P256, sizeof(OID_P256)))
                key->curve = DnieCurveNistP256;
        } else if(params.tag == 0x02 && der_uint(&params, &id)) {
            key->param_id = id; // dominio estandarizado (13 = brainpoolP256r1)
            key->curve = id == 13 ? DnieCurveBrainpoolP256r1 :
                         id == 12 ? DnieCurveNistP256 :
                                    DnieCurveUnknown;
        } else if(params.tag == 0x30) {
            key->explicit_params = params;
            key->curve = explicit_curve(&params);
        }
    }
    // BIT STRING: primer byte = bits no usados (0), luego el punto
    if(bits.len >= 2 && bits.val[0] == 0 && bits.len - 1 <= sizeof(key->public_key)) {
        memcpy(key->public_key, bits.val + 1, bits.len - 1);
        key->public_key_len = bits.len - 1;
    }
}

bool dnie_dg14_parse(const uint8_t* data, size_t len, DnieDg14* out) {
    memset(out, 0, sizeof(*out));
    DerIt it = der_it(data, len);
    DerTlv t6e, set, si;
    if(!der_expect(&it, 0x6E, &t6e)) return false;
    DerIt i = der_enter(&t6e);
    if(!der_expect(&i, 0x31, &set)) return false;

    DerIt infos = der_enter(&set);
    while(der_next(&infos, &si)) {
        if(si.tag != 0x30) continue;
        out->total_infos++;
        DerIt s = der_enter(&si);
        DerTlv oid, f1, f2;
        if(!der_expect(&s, 0x06, &oid)) continue;
        uint8_t a = 0, b = 0;
        int proto = bsi_proto(&oid, &a, &b);
        bool has_f1 = der_next(&s, &f1);
        bool has_f2 = der_next(&s, &f2);

        if(proto == BSI_CA && b != 0 && out->info_count < DNIE_DG14_MAX_CA) {
            // id-CA-{DH,ECDH}-{3DES,AES}: version, keyId opcional
            DnieCaInfo* ci = &out->infos[out->info_count++];
            ci->ka = a;
            ci->cipher = b;
            if(has_f1) der_uint(&f1, &ci->version);
            if(has_f2 && der_uint(&f2, &ci->key_id)) ci->has_key_id = true;
        } else if(proto == BSI_PK && out->key_count < DNIE_DG14_MAX_CA && has_f1 && f1.tag == 0x30) {
            DnieCaKey* k = &out->keys[out->key_count++];
            k->ka = a;
            parse_public_key(&f1, k);
            if(has_f2 && der_uint(&f2, &k->key_id)) k->has_key_id = true;
        } else if(proto == BSI_TA) {
            out->has_ta = true;
        } else if(proto == BSI_PACE) {
            out->has_pace = true;
        } else if(der_oid_is(&oid, OID_AA, sizeof(OID_AA))) {
            out->has_active_auth = true;
        }
    }
    return out->total_infos > 0;
}

const char* dnie_curve_name(DnieCurve c) {
    switch(c) {
    case DnieCurveBrainpoolP256r1:
        return "brainpoolP256r1";
    case DnieCurveNistP256:
        return "NIST P-256";
    case DnieCurveExplicit:
        return "explicit";
    default:
        return "?";
    }
}
