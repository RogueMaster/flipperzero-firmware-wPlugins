#include "pocsag.h"

#include <string.h>

/* Table des symboles numériques, index = valeur BCD (0xA..0xF : caractères spéciaux). */
static const char numeric_chars[] = "0123456789*U -)(";

uint32_t pocsag_bch(uint32_t cw) {
    cw &= 0xFFFFF800u; /* garde les 21 bits de données */

    /* Division polynomiale du mot de 31 bits (bits 31..1) par g(x) = 0x769. */
    uint32_t r = cw >> 1;
    for(int i = 30; i >= 10; i--) {
        if(r & (1u << i)) r ^= 0x769u << (i - 10);
    }
    cw |= (r & 0x3FFu) << 1;

    uint32_t p = cw;
    p ^= p >> 16;
    p ^= p >> 8;
    p ^= p >> 4;
    p ^= p >> 2;
    p ^= p >> 1;
    return cw | (p & 1u);
}

bool pocsag_char_valid(PocsagType type, char c) {
    if(type == PocsagNumeric) return c && strchr(numeric_chars, c);
    return c >= 0x20 && c <= 0x7E;
}

typedef struct {
    uint32_t* out;
    size_t max;
    size_t n;
    uint8_t slot; /* position dans le lot courant (0..16) */
    uint32_t data; /* bits de message en cours */
    uint8_t nbits;
    bool overflow;
} Enc;

static void put(Enc* e, uint32_t w) {
    if(e->slot == 16) {
        if(e->n < e->max) e->out[e->n++] = POCSAG_SYNC;
        e->slot = 0;
    }
    if(e->n < e->max)
        e->out[e->n++] = w;
    else
        e->overflow = true;
    e->slot++;
}

static void put_bit(Enc* e, uint32_t bit) {
    e->data = (e->data << 1) | bit;
    if(++e->nbits == 20) {
        put(e, pocsag_bch(0x80000000u | (e->data << 11)));
        e->data = 0;
        e->nbits = 0;
    }
}

/* Les symboles sont transmis bit de poids faible en premier. */
static void put_symbol(Enc* e, uint8_t v, uint8_t bits) {
    for(uint8_t i = 0; i < bits; i++)
        put_bit(e, (v >> i) & 1u);
}

size_t pocsag_encode(
    uint32_t ric,
    uint8_t func,
    PocsagType type,
    const char* msg,
    uint32_t* out,
    size_t max) {
    if(ric > POCSAG_RIC_MAX || func > 3 || !msg || strlen(msg) > POCSAG_MSG_MAX || max < 1)
        return 0;
    for(const char* c = msg; *c; c++)
        if(!pocsag_char_valid(type, *c)) return 0;

    Enc e = {.out = out, .max = max};
    out[e.n++] = POCSAG_SYNC;

    /* L'adresse ne peut être placée que dans la trame ric % 8 (2 mots par trame). */
    uint8_t frame = ric & 7u;
    while(e.slot < frame * 2)
        put(&e, POCSAG_IDLE);
    put(&e, pocsag_bch(((ric >> 3) << 13) | ((uint32_t)func << 11)));

    if(type == PocsagNumeric) {
        for(const char* c = msg; *c; c++)
            put_symbol(&e, (uint8_t)(strchr(numeric_chars, *c) - numeric_chars), 4);
        while(e.nbits)
            put_symbol(&e, 0xC, 4); /* bourrage avec des espaces */
    } else {
        for(const char* c = msg; *c; c++)
            put_symbol(&e, (uint8_t)*c, 7);
        while(e.nbits)
            put_bit(&e, 0); /* bourrage à zéro */
    }

    /* Fin de message : au moins un IDLE, puis compléter le lot. */
    put(&e, POCSAG_IDLE);
    while(e.slot < 16)
        put(&e, POCSAG_IDLE);

    return e.overflow ? 0 : e.n;
}
