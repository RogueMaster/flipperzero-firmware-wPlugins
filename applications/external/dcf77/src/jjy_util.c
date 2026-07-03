#include "jjy_util.h"

#include <string.h>

#define JJY_FRAME_SECONDS 60U

static void jjy_set_symbol(RadioClockPulse* frame, uint8_t bit, RadioClockPulse pulse) {
    frame[bit] = pulse;
}

static void jjy_set_weighted_decimal(
    RadioClockPulse* frame,
    uint16_t value,
    const uint8_t* bit_positions,
    const uint16_t* weights,
    size_t count) {
    uint16_t remainder = value;

    for(size_t i = 0; i < count; i++) {
        if(remainder >= weights[i]) {
            jjy_set_symbol(frame, bit_positions[i], RadioClockPulseOne);
            remainder = (uint16_t)(remainder - weights[i]);
        } else {
            jjy_set_symbol(frame, bit_positions[i], RadioClockPulseZero);
        }
    }
}

static bool jjy_parity(const RadioClockPulse* frame, const uint8_t* positions, size_t count) {
    bool parity = false;

    for(size_t i = 0; i < count; i++) {
        parity ^= (frame[positions[i]] == RadioClockPulseOne);
    }

    return parity;
}

static bool jjy_is_format_b_minute(uint8_t minute) {
    return minute == 15U || minute == 45U;
}

static void jjy_apply_format_b_final_block(RadioClockPulse* dest) {
    /* Official JJY format-B final block keeps second 40 at 0, sends JJY at
       seconds 41-48, keeps P5/P0 markers, then uses the interruption bits. */
    static const RadioClockPulse call_sign_pattern[] = {
        RadioClockPulseZero,
        RadioClockPulseZero,
        RadioClockPulseZero,
        RadioClockPulseOne,
        RadioClockPulseZero,
        RadioClockPulseOne,
        RadioClockPulseOne,
        RadioClockPulseZero,
    };

    dest[40] = RadioClockPulseZero;
    for(size_t i = 0; i < sizeof(call_sign_pattern) / sizeof(call_sign_pattern[0]); i++) {
        dest[41U + i] = call_sign_pattern[i];
    }
    dest[49] = RadioClockPulseMarker;

    /* Safe default: no planned interruption and no extra flags. */
    for(uint8_t second = 50U; second <= 58U; second++) {
        dest[second] = RadioClockPulseZero;
    }
    dest[59] = RadioClockPulseMarker;
}

void set_jjy_timecode(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint16_t day_of_year,
    uint8_t year,
    uint8_t weekday,
    bool su1,
    bool su2,
    bool leap_second_pending,
    bool leap_second_insert) {
    static const uint8_t marker_bits[] = {0, 9, 19, 29, 39, 49, 59};
    static const uint8_t minute_bits[] = {1, 2, 3, 5, 6, 7, 8};
    static const uint16_t minute_weights[] = {40, 20, 10, 8, 4, 2, 1};
    static const uint8_t hour_bits[] = {12, 13, 15, 16, 17, 18};
    static const uint16_t hour_weights[] = {20, 10, 8, 4, 2, 1};
    static const uint8_t doy_bits[] = {22, 23, 25, 26, 27, 28, 30, 31, 32, 33};
    static const uint16_t doy_weights[] = {200, 100, 80, 40, 20, 10, 8, 4, 2, 1};
    static const uint8_t year_bits[] = {41, 42, 43, 44, 45, 46, 47, 48};
    static const uint16_t year_weights[] = {80, 40, 20, 10, 8, 4, 2, 1};
    static const uint8_t weekday_bits[] = {50, 51, 52};
    static const uint16_t weekday_weights[] = {4, 2, 1};

    memset(dest, RadioClockPulseZero, sizeof(RadioClockPulse) * JJY_FRAME_SECONDS);

    for(size_t i = 0; i < sizeof(marker_bits) / sizeof(marker_bits[0]); i++) {
        jjy_set_symbol(dest, marker_bits[i], RadioClockPulseMarker);
    }

    jjy_set_weighted_decimal(
        dest, minute, minute_bits, minute_weights, sizeof(minute_bits) / sizeof(minute_bits[0]));
    jjy_set_weighted_decimal(
        dest, hour, hour_bits, hour_weights, sizeof(hour_bits) / sizeof(hour_bits[0]));
    jjy_set_weighted_decimal(
        dest, day_of_year, doy_bits, doy_weights, sizeof(doy_bits) / sizeof(doy_bits[0]));
    jjy_set_weighted_decimal(
        dest, year, year_bits, year_weights, sizeof(year_bits) / sizeof(year_bits[0]));
    jjy_set_weighted_decimal(
        dest,
        (uint8_t)(weekday % 7U),
        weekday_bits,
        weekday_weights,
        sizeof(weekday_bits) / sizeof(weekday_bits[0]));

    jjy_set_symbol(
        dest,
        36,
        jjy_parity(dest, hour_bits, sizeof(hour_bits) / sizeof(hour_bits[0])) ?
            RadioClockPulseOne :
            RadioClockPulseZero);
    jjy_set_symbol(
        dest,
        37,
        jjy_parity(dest, minute_bits, sizeof(minute_bits) / sizeof(minute_bits[0])) ?
            RadioClockPulseOne :
            RadioClockPulseZero);
    jjy_set_symbol(dest, 38, su1 ? RadioClockPulseOne : RadioClockPulseZero);
    jjy_set_symbol(dest, 40, su2 ? RadioClockPulseOne : RadioClockPulseZero);
    jjy_set_symbol(dest, 53, leap_second_pending ? RadioClockPulseOne : RadioClockPulseZero);
    jjy_set_symbol(dest, 54, leap_second_insert ? RadioClockPulseOne : RadioClockPulseZero);

    if(jjy_is_format_b_minute(minute)) {
        jjy_apply_format_b_final_block(dest);
        return;
    }

    jjy_set_symbol(dest, 55, RadioClockPulseZero);
    jjy_set_symbol(dest, 56, RadioClockPulseZero);
    jjy_set_symbol(dest, 57, RadioClockPulseZero);
    jjy_set_symbol(dest, 58, RadioClockPulseZero);
}
