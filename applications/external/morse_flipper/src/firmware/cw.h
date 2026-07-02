/*
 * Purpose: Publish host-safe Morse code encoding helpers.
 * Owns: CW_INVALID and the public cw_* conversion API.
 * Depends on: stdbool/stddef/stdint only.
 * Tests: tests/test_trainer.c and tests/test_keyer.c cover callers.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CW_INVALID 0xFFu

extern const uint8_t cw_ascii[127];

uint8_t cw(char c);
uint8_t cw_symbol_count(uint16_t cw_code);
uint8_t cw_symbol_units(uint16_t cw_code, uint8_t mark_idx);
uint8_t cw_total_units(uint16_t cw_code);
void cw_to_text(uint16_t cw_code, char* out, size_t out_sz);

/* expects uint8_t cw_char already loaded with cw() */
#define FOR_EACH_CW_SYMBOL(var)       \
    for(; cw_char > 1; cw_char >>= 1) \
        for(bool var = ((cw_char & 1u) != 0), _cw_once = true; _cw_once; _cw_once = false)
