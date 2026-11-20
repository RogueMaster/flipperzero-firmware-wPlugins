#pragma once

#include "zero_buster.h"

#ifdef __cplusplus
extern "C" {
#endif

/* keymap is a 128 entry ASCII -> HID usage table (built-in or loaded .kl). */
void zero_buster_type_char(char c, const uint16_t* keymap, uint32_t delay_ms);

void zero_buster_type_string(
    const char* str,
    const uint16_t* keymap,
    uint32_t delay_ms,
    volatile bool* stop_flag);

uint8_t get_hex_nibble(uint32_t val, uint8_t nibble_idx);
uint32_t set_hex_nibble(uint32_t val, uint8_t nibble_idx, uint8_t new_nibble);

#ifdef __cplusplus
}
#endif
