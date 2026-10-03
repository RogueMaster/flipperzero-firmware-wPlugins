#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t length;
    uint8_t wpm;
    uint8_t farnsworth_wpm;
} MorseFlipperRxSettings;

void morse_flipper_rx_settings_reset(MorseFlipperRxSettings* settings);
void morse_flipper_rx_settings_normalize(MorseFlipperRxSettings* settings);
void morse_flipper_rx_settings_length_bounds(
    uint8_t selection,
    uint8_t* min_length,
    uint8_t* max_length);
bool morse_flipper_rx_settings_load(MorseFlipperRxSettings* settings);
bool morse_flipper_rx_settings_save(const MorseFlipperRxSettings* settings);
