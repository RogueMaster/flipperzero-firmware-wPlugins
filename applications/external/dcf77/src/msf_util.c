#include "msf_util.h"

#include <string.h>

static void msf_set_bit(bool* bits, uint8_t index, bool value) {
    bits[index] = value;
}

static void msf_write_weighted_bits(
    bool* bits,
    uint16_t value,
    const uint8_t* positions,
    const uint8_t* weights,
    size_t count) {
    uint16_t remainder = value;

    for(size_t i = 0; i < count; i++) {
        if(remainder >= weights[i]) {
            msf_set_bit(bits, positions[i], true);
            remainder -= weights[i];
        } else {
            msf_set_bit(bits, positions[i], false);
        }
    }
}

static bool msf_range_has_odd_parity(const bool* bits, uint8_t start, uint8_t end_inclusive) {
    bool parity = false;

    for(uint8_t index = start; index <= end_inclusive; index++) {
        parity ^= bits[index];
    }

    return parity;
}

static void msf_set_dut1_bits(bool* bits_b, int8_t dut1_tenths) {
    if(dut1_tenths > 0) {
        for(uint8_t i = 0; i < (uint8_t)dut1_tenths && i < 8U; i++) {
            msf_set_bit(bits_b, (uint8_t)(1U + i), true);
        }
    } else if(dut1_tenths < 0) {
        uint8_t magnitude = (uint8_t)(-dut1_tenths);
        for(uint8_t i = 0; i < magnitude && i < 8U; i++) {
            msf_set_bit(bits_b, (uint8_t)(9U + i), true);
        }
    }
}

void set_msf_timecode(
    bool* bits_a,
    bool* bits_b,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year,
    uint8_t weekday,
    bool summer_time_active,
    bool summer_time_warning,
    int8_t dut1_tenths) {
    static const uint8_t year_positions[] = {17, 18, 19, 20, 21, 22, 23, 24};
    static const uint8_t year_weights[] = {80, 40, 20, 10, 8, 4, 2, 1};
    static const uint8_t month_positions[] = {25, 26, 27, 28, 29};
    static const uint8_t month_weights[] = {10, 8, 4, 2, 1};
    static const uint8_t day_positions[] = {30, 31, 32, 33, 34, 35};
    static const uint8_t day_weights[] = {20, 10, 8, 4, 2, 1};
    static const uint8_t weekday_positions[] = {36, 37, 38};
    static const uint8_t weekday_weights[] = {4, 2, 1};
    static const uint8_t hour_positions[] = {39, 40, 41, 42, 43, 44};
    static const uint8_t hour_weights[] = {20, 10, 8, 4, 2, 1};
    static const uint8_t minute_positions[] = {45, 46, 47, 48, 49, 50, 51};
    static const uint8_t minute_weights[] = {40, 20, 10, 8, 4, 2, 1};

    memset(bits_a, 0, sizeof(bool) * MSF_FRAME_SECONDS);
    memset(bits_b, 0, sizeof(bool) * MSF_FRAME_SECONDS);

    msf_set_bit(bits_a, 0, true);
    msf_set_bit(bits_b, 0, true);

    msf_set_dut1_bits(bits_b, dut1_tenths);

    msf_write_weighted_bits(
        bits_a,
        year,
        year_positions,
        year_weights,
        sizeof(year_positions) / sizeof(year_positions[0]));
    msf_write_weighted_bits(
        bits_a,
        month,
        month_positions,
        month_weights,
        sizeof(month_positions) / sizeof(month_positions[0]));
    msf_write_weighted_bits(
        bits_a, day, day_positions, day_weights, sizeof(day_positions) / sizeof(day_positions[0]));
    msf_write_weighted_bits(
        bits_a,
        weekday & 0x07U,
        weekday_positions,
        weekday_weights,
        sizeof(weekday_positions) / sizeof(weekday_positions[0]));
    msf_write_weighted_bits(
        bits_a,
        hour,
        hour_positions,
        hour_weights,
        sizeof(hour_positions) / sizeof(hour_positions[0]));
    msf_write_weighted_bits(
        bits_a,
        minute,
        minute_positions,
        minute_weights,
        sizeof(minute_positions) / sizeof(minute_positions[0]));

    msf_set_bit(bits_a, 53, true);
    msf_set_bit(bits_a, 54, true);
    msf_set_bit(bits_a, 55, true);
    msf_set_bit(bits_a, 56, true);
    msf_set_bit(bits_a, 57, true);
    msf_set_bit(bits_a, 58, true);

    msf_set_bit(bits_b, 53, summer_time_warning);
    msf_set_bit(bits_b, 54, !msf_range_has_odd_parity(bits_a, 17, 24));
    msf_set_bit(bits_b, 55, !msf_range_has_odd_parity(bits_a, 25, 35));
    msf_set_bit(bits_b, 56, !msf_range_has_odd_parity(bits_a, 36, 38));
    msf_set_bit(bits_b, 57, !msf_range_has_odd_parity(bits_a, 39, 51));
    msf_set_bit(bits_b, 58, summer_time_active);
}
