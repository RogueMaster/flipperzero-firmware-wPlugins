#ifndef PAIR_TIMECODE_H
#define PAIR_TIMECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "radio_clock_pulse.h"

RadioClockPulse pair_timecode_digit_to_pulse(uint8_t value);
uint8_t pair_timecode_pulse_to_digit(RadioClockPulse pulse);
void pair_timecode_fill_frame(RadioClockPulse* frame, size_t length, RadioClockPulse pulse);
void pair_timecode_set_symbol(RadioClockPulse* frame, uint8_t second, RadioClockPulse pulse);
void pair_timecode_set_bits(RadioClockPulse* frame, uint8_t second, bool high_bit, bool low_bit);
void pair_timecode_set_weighted(
    RadioClockPulse* frame,
    uint8_t second,
    uint16_t value,
    uint16_t high_weight,
    uint16_t low_weight);
bool pair_timecode_even_parity_bits(
    const RadioClockPulse* frame,
    uint8_t start,
    uint8_t end_inclusive);

#endif
