#pragma once

// Lector DER minimo (sin copias: apunta dentro del buffer original)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint32_t tag; // 1-3 bytes (p.ej. 0x30, 0xA0, 0x7F61)
    const uint8_t* raw; // inicio del TLV completo
    size_t raw_len;
    const uint8_t* val; // inicio del valor
    size_t len;
} DerTlv;

typedef struct {
    const uint8_t* p;
    const uint8_t* end;
} DerIt;

static inline DerIt der_it(const uint8_t* data, size_t len) {
    DerIt it = {data, data + len};
    return it;
}

static inline DerIt der_enter(const DerTlv* t) {
    return der_it(t->val, t->len);
}

// Siguiente TLV. false al final o si esta mal formado.
bool der_next(DerIt* it, DerTlv* out);

// Siguiente TLV que ademas tenga la etiqueta esperada
bool der_expect(DerIt* it, uint32_t tag, DerTlv* out);

// Compara un OID (valor del TLV 06) con su codificacion DER
bool der_oid_is(const DerTlv* oid, const uint8_t* enc, size_t enc_len);

// OID en texto con puntos ("1.2.840.10045.4.3.2")
void der_oid_str(const DerTlv* oid, char* out, size_t size);

// Entero pequeno (<= 4 bytes, sin signo)
bool der_uint(const DerTlv* t, uint32_t* out);
