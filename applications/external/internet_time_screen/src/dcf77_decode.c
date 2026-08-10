#include "dcf77_decode.h"

#include <stddef.h>

static uint8_t
    dcf77_bcd_bits(const uint8_t* bits, size_t start, const uint8_t* weights, size_t n) {
    uint8_t value = 0;
    for(size_t i = 0; i < n; i++) {
        if(bits[start + i]) {
            value = (uint8_t)(value + weights[i]);
        }
    }
    return value;
}

static bool dcf77_even_parity(const uint8_t* bits, size_t start, size_t count) {
    unsigned ones = 0;
    for(size_t i = 0; i < count; i++) {
        if(bits[start + i]) {
            ones++;
        }
    }
    return (ones % 2u) == 0u;
}

bool dcf77_decode_frame(const uint8_t bits[DCF77_FRAME_BITS], Dcf77CivilTime* out) {
    if(!bits || !out) {
        return false;
    }

    /* Bit 0 must be 0; bit 20 marks start of time encoding and must be 1. */
    if(bits[0] != 0 || bits[20] != 1) {
        return false;
    }

    const bool cest = bits[17] != 0;
    const bool cet = bits[18] != 0;
    if(cest == cet) {
        return false; /* exactly one of CET/CEST must be set */
    }

    static const uint8_t min_w[] = {1, 2, 4, 8, 10, 20, 40};
    static const uint8_t hour_w[] = {1, 2, 4, 8, 10, 20};
    static const uint8_t day_w[] = {1, 2, 4, 8, 10, 20};
    static const uint8_t wday_w[] = {1, 2, 4};
    static const uint8_t month_w[] = {1, 2, 4, 8, 10};
    static const uint8_t year_w[] = {1, 2, 4, 8, 10, 20, 40, 80};

    if(!dcf77_even_parity(bits, 21, 8)) { /* minutes + P1 */
        return false;
    }
    if(!dcf77_even_parity(bits, 29, 7)) { /* hours + P2 */
        return false;
    }
    if(!dcf77_even_parity(bits, 36, 23)) { /* date fields + P3 */
        return false;
    }

    const uint8_t minute = dcf77_bcd_bits(bits, 21, min_w, 7);
    const uint8_t hour = dcf77_bcd_bits(bits, 29, hour_w, 6);
    const uint8_t day = dcf77_bcd_bits(bits, 36, day_w, 6);
    const uint8_t weekday = dcf77_bcd_bits(bits, 42, wday_w, 3);
    const uint8_t month = dcf77_bcd_bits(bits, 45, month_w, 5);
    const uint8_t year_yy = dcf77_bcd_bits(bits, 50, year_w, 8);

    if(minute > 59 || hour > 23 || day < 1 || day > 31 || month < 1 || month > 12 || weekday < 1 ||
       weekday > 7 || year_yy > 99) {
        return false;
    }

    out->minute = minute;
    out->hour = hour;
    out->day = day;
    out->month = month;
    out->year = (uint16_t)(2000u + year_yy);
    out->weekday = weekday;
    out->cest = cest;
    return true;
}
