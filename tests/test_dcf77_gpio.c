#include "dcf77_gpio.h"

#include <stdio.h>

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

static void test_pulse_widths(void) {
    expect_true("100ms zero", dcf77_classify_pulse_ms(100) == Dcf77PulseZero);
    expect_true("200ms one", dcf77_classify_pulse_ms(200) == Dcf77PulseOne);
    expect_true("50ms noise", dcf77_classify_pulse_ms(50) == Dcf77PulseNoise);
    expect_true("1500ms gap minute", dcf77_classify_gap_ms(1500) == Dcf77PulseMinuteMark);
    expect_true("1600ms gap minute", dcf77_classify_gap_ms(1600) == Dcf77PulseMinuteMark);
    expect_true("800ms gap noise", dcf77_classify_gap_ms(800) == Dcf77PulseNoise);
}

static void test_buffer_minute(void) {
    Dcf77BitBuffer buf;
    dcf77_bit_buffer_reset(&buf);
    for(uint8_t i = 0; i < DCF77_FRAME_BITS; i++) {
        expect_true(
            "feed bit", !dcf77_bit_buffer_feed(&buf, (i % 2) ? Dcf77PulseOne : Dcf77PulseZero));
    }
    expect_eq_u8("59 bits", buf.count, DCF77_FRAME_BITS);
    expect_true("minute mark completes", dcf77_bit_buffer_feed(&buf, Dcf77PulseMinuteMark));
    expect_true("complete flag", buf.complete);
}

int test_dcf77_gpio_run(void) {
    failures = 0;
    test_pulse_widths();
    test_buffer_minute();
    return failures;
}
