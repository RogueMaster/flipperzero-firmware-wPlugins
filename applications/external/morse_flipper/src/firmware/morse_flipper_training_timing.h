#pragma once

#include <stdint.h>

uint16_t morse_flipper_training_char_gap_ms(
    uint16_t dit_ms,
    uint8_t character_wpm,
    uint8_t farnsworth_wpm);
