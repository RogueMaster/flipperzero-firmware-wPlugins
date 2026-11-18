#pragma once

#include "zero_buster.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Type a single character through USB HID taking layout into account */
void zero_buster_type_char(char c, KeyboardLayout layout, uint32_t delay_ms);

/* Type an entire string through USB HID */
void zero_buster_type_string(
    const char* str,
    KeyboardLayout layout,
    uint32_t delay_ms,
    volatile bool* stop_flag);

/* Hex nibble helpers for in-app HID editor */
uint8_t get_hex_nibble(uint32_t val, uint8_t nibble_idx);
uint32_t set_hex_nibble(uint32_t val, uint8_t nibble_idx, uint8_t new_nibble);

#ifdef __cplusplus
}
#endif
