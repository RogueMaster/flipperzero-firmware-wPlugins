#include "dcf77_decode.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(const char* name, bool cond) {
    if(!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void expect_eq_u8(const char* name, uint8_t got, uint8_t want) {
    if(got != want) {
        fprintf(stderr, "FAIL: %s (got %u, want %u)\n", name, got, want);
        failures++;
    }
}

static void expect_eq_u16(const char* name, uint16_t got, uint16_t want) {
    if(got != want) {
        fprintf(stderr, "FAIL: %s (got %u, want %u)\n", name, got, want);
        failures++;
    }
}

static void set_bcd(
    uint8_t bits[DCF77_FRAME_BITS],
    size_t start,
    const uint8_t* weights,
    size_t n,
    uint8_t value) {
    for(size_t i = 0; i < n; i++) {
        bits[start + i] = 0;
    }
    for(size_t i = n; i-- > 0;) {
        if(value >= weights[i]) {
            bits[start + i] = 1;
            value = (uint8_t)(value - weights[i]);
        }
    }
}

static void set_even_parity(uint8_t bits[DCF77_FRAME_BITS], size_t start, size_t data_count) {
    unsigned ones = 0;
    for(size_t i = 0; i < data_count; i++) {
        if(bits[start + i]) {
            ones++;
        }
    }
    bits[start + data_count] = (ones % 2u) != 0u ? 1 : 0;
}

static void build_frame(
    uint8_t bits[DCF77_FRAME_BITS],
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t weekday,
    uint8_t month,
    uint8_t year_yy,
    bool cest) {
    memset(bits, 0, DCF77_FRAME_BITS);
    bits[0] = 0;
    bits[20] = 1;
    bits[17] = cest ? 1 : 0;
    bits[18] = cest ? 0 : 1;

    static const uint8_t min_w[] = {1, 2, 4, 8, 10, 20, 40};
    static const uint8_t hour_w[] = {1, 2, 4, 8, 10, 20};
    static const uint8_t day_w[] = {1, 2, 4, 8, 10, 20};
    static const uint8_t wday_w[] = {1, 2, 4};
    static const uint8_t month_w[] = {1, 2, 4, 8, 10};
    static const uint8_t year_w[] = {1, 2, 4, 8, 10, 20, 40, 80};

    set_bcd(bits, 21, min_w, 7, minute);
    set_even_parity(bits, 21, 7);
    set_bcd(bits, 29, hour_w, 6, hour);
    set_even_parity(bits, 29, 6);
    set_bcd(bits, 36, day_w, 6, day);
    set_bcd(bits, 42, wday_w, 3, weekday);
    set_bcd(bits, 45, month_w, 5, month);
    set_bcd(bits, 50, year_w, 8, year_yy);
    set_even_parity(bits, 36, 22);
}

static void test_valid_cet_midnight(void) {
    uint8_t bits[DCF77_FRAME_BITS];
    /* Monday 2026-08-10 00:00 CET */
    build_frame(bits, 0, 0, 10, 1, 8, 26, false);
    Dcf77CivilTime t;
    expect_true("CET midnight decode", dcf77_decode_frame(bits, &t));
    expect_eq_u8("CET midnight hour", t.hour, 0);
    expect_eq_u8("CET midnight minute", t.minute, 0);
    expect_eq_u8("CET midnight day", t.day, 10);
    expect_eq_u8("CET midnight month", t.month, 8);
    expect_eq_u16("CET midnight year", t.year, 2026);
    expect_eq_u8("CET midnight weekday", t.weekday, 1);
    expect_true("CET midnight not CEST", !t.cest);
}

static void test_valid_cest_noon(void) {
    uint8_t bits[DCF77_FRAME_BITS];
    build_frame(bits, 0, 12, 10, 1, 8, 26, true);
    Dcf77CivilTime t;
    expect_true("CEST noon decode", dcf77_decode_frame(bits, &t));
    expect_eq_u8("CEST noon hour", t.hour, 12);
    expect_eq_u8("CEST noon minute", t.minute, 0);
    expect_true("CEST noon flag", t.cest);
}

static void test_bad_minute_parity(void) {
    uint8_t bits[DCF77_FRAME_BITS];
    build_frame(bits, 30, 12, 10, 1, 8, 26, true);
    bits[28] ^= 1; /* flip P1 */
    Dcf77CivilTime t;
    expect_true("bad minute parity rejected", !dcf77_decode_frame(bits, &t));
}

static void test_incomplete_frame_start_bits(void) {
    uint8_t bits[DCF77_FRAME_BITS];
    build_frame(bits, 0, 0, 1, 1, 1, 26, false);
    bits[20] = 0; /* missing time-start marker */
    Dcf77CivilTime t;
    expect_true("incomplete/start rejected", !dcf77_decode_frame(bits, &t));
}

int test_dcf77_decode_run(void) {
    failures = 0;
    test_valid_cet_midnight();
    test_valid_cest_noon();
    test_bad_minute_parity();
    test_incomplete_frame_start_bits();
    return failures;
}
