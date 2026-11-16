#pragma once

#include <stddef.h>
#include <stdint.h>

static inline void dfc_cmac_xor_in_place(uint8_t* value, const uint8_t* mask, size_t length) {
    for(size_t i = 0; i < length; i++) {
        value[i] ^= mask[i];
    }
}
