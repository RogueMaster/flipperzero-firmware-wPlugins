#include "wwvb_util.h"

#include <string.h>

#define WWVB_FRAME_SECONDS 60U

static void wwvb_set_symbol(RadioClockPulse* frame, uint8_t bit, RadioClockPulse pulse) {
    frame[bit] = pulse;
}

static void wwvb_set_weighted_decimal(
    RadioClockPulse* frame,
    uint16_t value,
    const uint8_t* bit_positions,
    const uint8_t* weights,
    size_t count) {
    uint16_t remainder = value;

    for(size_t i = 0; i < count; i++) {
        if(remainder >= weights[i]) {
            wwvb_set_symbol(frame, bit_positions[i], RadioClockPulseOne);
            remainder -= weights[i];
        } else {
            wwvb_set_symbol(frame, bit_positions[i], RadioClockPulseZero);
        }
    }
}

bool wwvb_is_leap_year(uint16_t year) {
    return ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
}

uint16_t wwvb_day_of_year(uint16_t year, uint8_t month, uint8_t day) {
    static const uint16_t days_before_month[] = {
        0U,
        31U,
        59U,
        90U,
        120U,
        151U,
        181U,
        212U,
        243U,
        273U,
        304U,
        334U,
    };

    uint16_t result = days_before_month[month - 1U] + day;
    if(month > 2U && wwvb_is_leap_year(year)) {
        result++;
    }

    return result;
}

void set_wwvb_am_symbols(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint16_t day_of_year,
    uint8_t year,
    bool dst_active,
    bool dst_recent_change,
    bool leap_year,
    bool leap_second,
    int8_t ut1_correction_tenths) {
    static const uint8_t marker_bits[] = {0, 9, 19, 29, 39, 49, 59};
    static const uint8_t minute_bits[] = {1, 2, 3, 5, 6, 7, 8};
    static const uint8_t minute_weights[] = {40, 20, 10, 8, 4, 2, 1};
    static const uint8_t hour_bits[] = {12, 13, 15, 16, 17, 18};
    static const uint8_t hour_weights[] = {20, 10, 8, 4, 2, 1};
    static const uint8_t doy_bits[] = {22, 23, 25, 26, 27, 28, 30, 31, 32, 33};
    static const uint8_t doy_weights[] = {200, 100, 80, 40, 20, 10, 8, 4, 2, 1};
    static const uint8_t year_bits[] = {45, 46, 47, 48, 50, 51, 52, 53};
    static const uint8_t year_weights[] = {80, 40, 20, 10, 8, 4, 2, 1};
    uint8_t ut1_magnitude;

    memset(dest, RadioClockPulseZero, sizeof(RadioClockPulse) * WWVB_FRAME_SECONDS);

    for(size_t i = 0; i < (sizeof(marker_bits) / sizeof(marker_bits[0])); i++) {
        wwvb_set_symbol(dest, marker_bits[i], RadioClockPulseMarker);
    }

    wwvb_set_weighted_decimal(
        dest, minute, minute_bits, minute_weights, sizeof(minute_bits) / sizeof(minute_bits[0]));
    wwvb_set_weighted_decimal(
        dest, hour, hour_bits, hour_weights, sizeof(hour_bits) / sizeof(hour_bits[0]));
    wwvb_set_weighted_decimal(
        dest, day_of_year, doy_bits, doy_weights, sizeof(doy_bits) / sizeof(doy_bits[0]));
    wwvb_set_weighted_decimal(
        dest, year, year_bits, year_weights, sizeof(year_bits) / sizeof(year_bits[0]));

    if(ut1_correction_tenths > 0) {
        wwvb_set_symbol(dest, 36, RadioClockPulseOne);
        wwvb_set_symbol(dest, 38, RadioClockPulseOne);
    } else if(ut1_correction_tenths < 0) {
        wwvb_set_symbol(dest, 37, RadioClockPulseOne);
    }

    ut1_magnitude =
        (uint8_t)(ut1_correction_tenths < 0 ? -ut1_correction_tenths : ut1_correction_tenths);
    wwvb_set_symbol(dest, 40, (ut1_magnitude & 0x08U) ? RadioClockPulseOne : RadioClockPulseZero);
    wwvb_set_symbol(dest, 41, (ut1_magnitude & 0x04U) ? RadioClockPulseOne : RadioClockPulseZero);
    wwvb_set_symbol(dest, 42, (ut1_magnitude & 0x02U) ? RadioClockPulseOne : RadioClockPulseZero);
    wwvb_set_symbol(dest, 43, (ut1_magnitude & 0x01U) ? RadioClockPulseOne : RadioClockPulseZero);

    wwvb_set_symbol(dest, 55, leap_year ? RadioClockPulseOne : RadioClockPulseZero);
    wwvb_set_symbol(dest, 56, leap_second ? RadioClockPulseOne : RadioClockPulseZero);
    wwvb_set_symbol(dest, 57, dst_recent_change ? RadioClockPulseOne : RadioClockPulseZero);
    wwvb_set_symbol(dest, 58, dst_active ? RadioClockPulseOne : RadioClockPulseZero);
}
