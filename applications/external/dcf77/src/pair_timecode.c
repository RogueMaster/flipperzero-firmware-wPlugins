#include "pair_timecode.h"

RadioClockPulse pair_timecode_digit_to_pulse(uint8_t value) {
    switch(value & 0x03U) {
    case 1U:
        return RadioClockPulsePair01;
    case 2U:
        return RadioClockPulsePair10;
    case 3U:
        return RadioClockPulsePair11;
    case 0U:
    default:
        return RadioClockPulsePair00;
    }
}

uint8_t pair_timecode_pulse_to_digit(RadioClockPulse pulse) {
    switch(pulse) {
    case RadioClockPulsePair01:
        return 1U;
    case RadioClockPulsePair10:
        return 2U;
    case RadioClockPulsePair11:
        return 3U;
    case RadioClockPulsePair00:
    default:
        return 0U;
    }
}

void pair_timecode_fill_frame(RadioClockPulse* frame, size_t length, RadioClockPulse pulse) {
    for(size_t i = 0; i < length; i++) {
        frame[i] = pulse;
    }
}

void pair_timecode_set_symbol(RadioClockPulse* frame, uint8_t second, RadioClockPulse pulse) {
    frame[second] = pulse;
}

void pair_timecode_set_bits(RadioClockPulse* frame, uint8_t second, bool high_bit, bool low_bit) {
    pair_timecode_set_symbol(
        frame,
        second,
        pair_timecode_digit_to_pulse((uint8_t)((high_bit ? 0x02U : 0U) | (low_bit ? 0x01U : 0U))));
}

void pair_timecode_set_weighted(
    RadioClockPulse* frame,
    uint8_t second,
    uint16_t value,
    uint16_t high_weight,
    uint16_t low_weight) {
    pair_timecode_set_bits(frame, second, (value & high_weight) != 0U, (value & low_weight) != 0U);
}

bool pair_timecode_even_parity_bits(
    const RadioClockPulse* frame,
    uint8_t start,
    uint8_t end_inclusive) {
    bool parity = false;

    for(uint8_t second = start; second <= end_inclusive; second++) {
        const uint8_t digit = pair_timecode_pulse_to_digit(frame[second]);
        parity ^= (digit & 0x02U) != 0U;
        parity ^= (digit & 0x01U) != 0U;
    }

    return parity;
}
