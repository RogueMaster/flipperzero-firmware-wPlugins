/*
 * Purpose: Encode and inspect International Morse code symbols.
 * Owns: ASCII-to-CW lookup data and symbol/unit helpers.
 * Depends on: cw.h and host-safe integer types.
 * Tests: tests/test_trainer.c and tests/test_keyer.c cover callers.
 */

#include "cw.h"

// clang-format off
const uint8_t cw_ascii[127] = {
    [0 ... 31] = 0xFF,

    0x01, 0x75, 0x52, 0xFF, 0xFF, 0xFF, 0x22, 0x5E, //  !"#$%&'
    0x2D, 0x6D, 0xFF, 0x2A, 0x73, 0x61, 0x6A, 0x29, // ()*+,-./
    0x3F, 0x3E, 0x3C, 0x38, 0x30, 0x20, 0x21, 0x23, // 01234567
    0x27, 0x2F, 0x47, 0x55, 0xFF, 0x31, 0xFF, 0x4C, // 89:;<=>?
    0x56, 0x06, 0x11, 0x15, 0x09, 0x02, 0x14, 0x0B, // @ABCDEFG
    0x10, 0x04, 0x1E, 0x0D, 0x12, 0x07, 0x05, 0x0F, // HIJKLMNO
    0x16, 0x1B, 0x0A, 0x08, 0x03, 0x0C, 0x18, 0x0E, // PQRSTUVW
    0x19, 0x1D, 0x13, 0xFF, 0xFF, 0xFF, 0xFF, 0x6C, // XYZ[\]^_
    0xFF, 0x06, 0x11, 0x15, 0x09, 0x02, 0x14, 0x0B, // `abcdefg
    0x10, 0x04, 0x1E, 0x0D, 0x12, 0x07, 0x05, 0x0F, // hijklmno
    0x16, 0x1B, 0x0A, 0x08, 0x03, 0x0C, 0x18, 0x0E, // pqrstuvw
    0x19, 0x1D, 0x13, 0xFF, 0xFF, 0xFF, 0xFF,       // xyz{|}~
};
// clang-format on

uint8_t cw(char c) {
    uint8_t a;

    a = (uint8_t)c;
    if(a >= sizeof(cw_ascii)) return CW_INVALID;

    return cw_ascii[a];
}

uint8_t cw_symbol_count(uint16_t cw_code) {
    uint8_t marks = 0U;

    if(cw_code == CW_INVALID) return 0U;

    while(cw_code > 1U) {
        marks++;
        cw_code >>= 1U;
    }

    return marks;
}

uint8_t cw_symbol_units(uint16_t cw_code, uint8_t mark_idx) {
    if(cw_code == CW_INVALID || mark_idx >= cw_symbol_count(cw_code)) return 0U;
    return ((cw_code >> mark_idx) & 1U) ? 3U : 1U;
}

uint8_t cw_total_units(uint16_t cw_code) {
    uint8_t marks = cw_symbol_count(cw_code);
    uint8_t total = 0U;

    for(uint8_t i = 0U; i < marks; i++) {
        total = (uint8_t)(total + cw_symbol_units(cw_code, i));
        if(i + 1U < marks) total++;
    }

    return total;
}

void cw_to_text(uint16_t cw_code, char* out, size_t out_sz) {
    uint8_t marks;
    uint8_t i;

    if(out == NULL || out_sz == 0U) return;
    out[0] = '\0';

    marks = cw_symbol_count(cw_code);
    for(i = 0U; i < marks && i + 1U < out_sz; i++) {
        out[i] = cw_symbol_units(cw_code, i) == 3U ? '-' : '.';
    }
    out[i] = '\0';
}
