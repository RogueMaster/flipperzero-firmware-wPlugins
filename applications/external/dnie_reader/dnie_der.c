#include "dnie_der.h"

#include <stdio.h>
#include <string.h>

bool der_next(DerIt* it, DerTlv* out) {
    const uint8_t* p = it->p;
    const uint8_t* end = it->end;
    if(p >= end) return false;
    out->raw = p;

    // Etiqueta: forma alta si los 5 bits bajos son 11111 (hasta 3 bytes en total)
    uint32_t tag = *p++;
    if((tag & 0x1F) == 0x1F) {
        int extra = 0;
        do {
            if(p >= end || ++extra > 2) return false;
            tag = (tag << 8) | *p;
        } while(*p++ & 0x80);
    }

    // Longitud: corta o larga hasta 3 bytes
    if(p >= end) return false;
    size_t len = *p++;
    if(len & 0x80) {
        int n = len & 0x7F;
        if(n == 0 || n > 3 || end - p < n) return false;
        len = 0;
        while(n--)
            len = (len << 8) | *p++;
    }
    if((size_t)(end - p) < len) return false;

    out->tag = tag;
    out->val = p;
    out->len = len;
    out->raw_len = (size_t)(p + len - out->raw);
    it->p = p + len;
    return true;
}

bool der_expect(DerIt* it, uint32_t tag, DerTlv* out) {
    return der_next(it, out) && out->tag == tag;
}

bool der_oid_is(const DerTlv* oid, const uint8_t* enc, size_t enc_len) {
    return oid->tag == 0x06 && oid->len == enc_len && memcmp(oid->val, enc, enc_len) == 0;
}

void der_oid_str(const DerTlv* oid, char* out, size_t size) {
    size_t pos = 0;
    out[0] = '\0';
    if(oid->tag != 0x06 || oid->len == 0) return;
    uint32_t v = 0;
    bool first = true;
    for(size_t i = 0; i < oid->len && pos + 12 < size; i++) {
        v = (v << 7) | (oid->val[i] & 0x7F);
        if(oid->val[i] & 0x80) continue;
        if(first) {
            uint32_t a = v < 40 ? 0 : (v < 80 ? 1 : 2);
            pos += (size_t)snprintf(
                out + pos, size - pos, "%lu.%lu", (unsigned long)a, (unsigned long)(v - 40 * a));
            first = false;
        } else {
            pos += (size_t)snprintf(out + pos, size - pos, ".%lu", (unsigned long)v);
        }
        v = 0;
    }
}

bool der_uint(const DerTlv* t, uint32_t* out) {
    if(t->tag != 0x02 || t->len == 0 || t->len > 5) return false;
    size_t i = 0;
    if(t->len == 5) {
        if(t->val[0] != 0) return false;
        i = 1;
    }
    uint32_t v = 0;
    for(; i < t->len; i++)
        v = (v << 8) | t->val[i];
    *out = v;
    return true;
}
