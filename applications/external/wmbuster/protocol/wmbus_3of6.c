#include "wmbus_3of6.h"

/* EN 13757-4 Table 11: data nibble -> 6-bit chip code. */
static const uint8_t k_nibble_to_chip[16] = {
    0x16, 0x0D, 0x0E, 0x0B, 0x1C, 0x19, 0x1A, 0x13,
    0x2C, 0x25, 0x26, 0x23, 0x34, 0x31, 0x32, 0x29,
};

/* Inverse table generated at first use, indexed by 6-bit chip value (0..63).
 * 0xFF means "not a valid chip". The CC1101 may decide some bits in error;
 * we still return 0xFF in that case so the caller can treat as a soft error. */
static uint8_t s_chip_to_nibble[64];
static bool s_inv_ready;

static void build_inverse(void) {
    for(int i = 0; i < 64; i++) s_chip_to_nibble[i] = 0xFF;
    for(int n = 0; n < 16; n++) s_chip_to_nibble[k_nibble_to_chip[n]] = (uint8_t)n;
    s_inv_ready = true;
}

uint8_t wmbus_3of6_decode_chip(uint8_t chip6) {
    if(!s_inv_ready) build_inverse();
    return s_chip_to_nibble[chip6 & 0x3F];
}

size_t wmbus_3of6_decode(
    const uint8_t* chips,
    size_t chip_bits,
    uint8_t* out,
    size_t out_capacity,
    bool* err) {
    if(!s_inv_ready) build_inverse();
    if(err) *err = false;

    /* We need 12 bits per output byte (two chips). */
    size_t n_bytes = chip_bits / 12;
    if(n_bytes > out_capacity) n_bytes = out_capacity;

    for(size_t i = 0; i < n_bytes; i++) {
        size_t bit = i * 12;
        uint8_t hi6 = 0, lo6 = 0;
        for(int k = 0; k < 6; k++) {
            size_t b = bit + k;
            uint8_t v = (chips[b >> 3] >> (7 - (b & 7))) & 1u;
            hi6 = (uint8_t)((hi6 << 1) | v);
        }
        for(int k = 0; k < 6; k++) {
            size_t b = bit + 6 + k;
            uint8_t v = (chips[b >> 3] >> (7 - (b & 7))) & 1u;
            lo6 = (uint8_t)((lo6 << 1) | v);
        }
        uint8_t hi = s_chip_to_nibble[hi6];
        uint8_t lo = s_chip_to_nibble[lo6];
        if(hi == 0xFF || lo == 0xFF) {
            if(err) *err = true;
            /* Fill nibble with 0 to keep stream length stable for debugging. */
            if(hi == 0xFF) hi = 0;
            if(lo == 0xFF) lo = 0;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return n_bytes;
}
