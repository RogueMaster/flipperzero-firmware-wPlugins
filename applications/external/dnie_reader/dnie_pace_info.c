#include "dnie_pace_info.h"

// id-PACE = 0.4.0.127.0.7.2.2.4  ->  codificado en DER: 04 00 7F 00 07 02 02 04
static const uint8_t ID_PACE_PREFIX[] = {0x04, 0x00, 0x7F, 0x00, 0x07, 0x02, 0x02, 0x04};

// Lector TLV minimo (tags de un byte, longitudes cortas, 0x81 y 0x82)
typedef struct {
    uint8_t tag;
    const uint8_t* value;
    size_t len;
} Tlv;

static bool tlv_next(const uint8_t** p, const uint8_t* end, Tlv* tlv) {
    if(end - *p < 2) return false;
    tlv->tag = *(*p)++;
    uint8_t l = *(*p)++;
    size_t len;
    if(l < 0x80) {
        len = l;
    } else if(l == 0x81 && end - *p >= 1) {
        len = *(*p)++;
    } else if(l == 0x82 && end - *p >= 2) {
        len = ((size_t)(*p)[0] << 8) | (*p)[1];
        *p += 2;
    } else {
        return false;
    }
    if((size_t)(end - *p) < len) return false;
    tlv->value = *p;
    tlv->len = len;
    *p += len;
    return true;
}

static bool tlv_to_uint(const Tlv* tlv, uint32_t* out) {
    if(tlv->tag != 0x02 || tlv->len == 0 || tlv->len > 4) return false;
    uint32_t v = 0;
    for(size_t i = 0; i < tlv->len; i++)
        v = (v << 8) | tlv->value[i];
    *out = v;
    return true;
}

size_t dnie_pace_info_parse(const uint8_t* data, size_t len, DniePaceInfo* out, size_t max_out) {
    const uint8_t* p = data;
    const uint8_t* end = data + len;
    Tlv set;
    if(!tlv_next(&p, end, &set) || set.tag != 0x31) return 0; // SET OF SecurityInfo

    size_t count = 0;
    const uint8_t* sp = set.value;
    const uint8_t* send = set.value + set.len;
    Tlv seq;

    while(count < max_out && tlv_next(&sp, send, &seq)) {
        if(seq.tag != 0x30) continue; // cada SecurityInfo es un SEQUENCE

        const uint8_t* q = seq.value;
        const uint8_t* qend = seq.value + seq.len;
        Tlv oid, version, param;

        if(!tlv_next(&q, qend, &oid) || oid.tag != 0x06) continue;
        // PACEInfo: prefijo id-PACE + 2 arcos (mapping, cipher)
        if(oid.len != sizeof(ID_PACE_PREFIX) + 2) continue;
        if(memcmp(oid.value, ID_PACE_PREFIX, sizeof(ID_PACE_PREFIX)) != 0) continue;

        DniePaceInfo* info = &out[count];
        memset(info, 0, sizeof(*info));
        info->mapping = oid.value[8];
        info->cipher = oid.value[9];

        if(!tlv_next(&q, qend, &version) || !tlv_to_uint(&version, &info->version)) continue;
        if(tlv_next(&q, qend, &param) && tlv_to_uint(&param, &info->param_id)) {
            info->has_param_id = true;
        }
        count++;
    }
    return count;
}

const char* dnie_pace_mapping_str(uint8_t mapping) {
    switch(mapping) {
    case 1:
        return "DH-GM";
    case 2:
        return "ECDH-GM";
    case 3:
        return "DH-IM";
    case 4:
        return "ECDH-IM";
    case 6:
        return "ECDH-CAM";
    }
    return "?";
}

const char* dnie_pace_cipher_str(uint8_t cipher) {
    switch(cipher) {
    case 1:
        return "3DES";
    case 2:
        return "AES-128";
    case 3:
        return "AES-192";
    case 4:
        return "AES-256";
    }
    return "?";
}

// Parametros de dominio estandarizados (TR-03110-3, tabla de parameterId)
const char* dnie_pace_param_str(uint32_t param_id) {
    switch(param_id) {
    case 0:
        return "MODP-1024/160";
    case 1:
        return "MODP-2048/224";
    case 2:
        return "MODP-2048/256";
    case 8:
        return "NIST P-192";
    case 9:
        return "brainpoolP192r1";
    case 10:
        return "NIST P-224";
    case 11:
        return "brainpoolP224r1";
    case 12:
        return "NIST P-256";
    case 13:
        return "brainpoolP256r1";
    case 14:
        return "brainpoolP320r1";
    case 15:
        return "NIST P-384";
    case 16:
        return "brainpoolP384r1";
    case 17:
        return "brainpoolP512r1";
    case 18:
        return "NIST P-521";
    }
    return "unknown";
}
