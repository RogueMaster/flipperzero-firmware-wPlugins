#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CW_INVALID 0xFFu

extern const uint8_t cw_ascii[127];

uint8_t cw(char c);

/* expects uint8_t cw_char already loaded with cw() */
#define FOR_EACH_CW_SYMBOL(var)       \
    for(; cw_char > 1; cw_char >>= 1) \
        for(bool var = ((cw_char & 1u) != 0), _cw_once = true; _cw_once; _cw_once = false)
