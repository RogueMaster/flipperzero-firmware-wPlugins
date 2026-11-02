#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

char flipper_blinker_morse_normalize_char(char c);
bool flipper_blinker_morse_is_supported(char c);
const char* flipper_blinker_morse_get(char c);
size_t flipper_blinker_morse_count_supported(const char* text);
uint32_t flipper_blinker_morse_unit_ms(uint8_t wpm);
