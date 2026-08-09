#include "internet_time.h"

#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

static void expect_true(const char* name, bool cond) {
    if(!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void expect_eq_u16(const char* name, uint16_t got, uint16_t want) {
    if(got != want) {
        fprintf(stderr, "FAIL: %s (got %u, want %u)\n", name, got, want);
        failures++;
    }
}

static void test_offset_validation(void) {
    expect_true("offset -12:00 valid", internet_time_offset_valid(-12 * 60));
    expect_true("offset +14:00 valid", internet_time_offset_valid(14 * 60));
    expect_true("offset +00:00 valid", internet_time_offset_valid(0));
    expect_true("offset +05:30 valid", internet_time_offset_valid(5 * 60 + 30));
    expect_true("offset -08:00 valid", internet_time_offset_valid(-8 * 60));
    expect_true("offset +01:00 valid", internet_time_offset_valid(60));

    expect_true("offset -12:15 invalid", !internet_time_offset_valid(-12 * 60 - 15));
    expect_true("offset +14:15 invalid", !internet_time_offset_valid(14 * 60 + 15));
    expect_true("offset +00:07 invalid", !internet_time_offset_valid(7));
    expect_true("offset +00:20 invalid", !internet_time_offset_valid(20));
}

static void test_bmt_boundaries(void) {
    expect_eq_u16("BMT midnight @000", internet_time_beats_from_bmt_seconds(0), 0);
    expect_eq_u16("BMT noon @500", internet_time_beats_from_bmt_seconds(12 * 3600), 500);
    expect_eq_u16(
        "BMT last second @999",
        internet_time_beats_from_bmt_seconds(INTERNET_TIME_DAY_SECONDS - 1),
        999);
    expect_eq_u16(
        "BMT wrap +86400 -> @000",
        internet_time_beats_from_bmt_seconds(INTERNET_TIME_DAY_SECONDS),
        0);
    expect_eq_u16("BMT wrap -1 -> @999", internet_time_beats_from_bmt_seconds(-1), 999);
}

static void test_local_conversion_fixtures(void) {
    /* At UTC+01:00, local == BMT. */
    expect_eq_u16("UTC+1 local midnight -> @000", internet_time_beats_from_local(0, 0, 0, 60), 0);
    expect_eq_u16("UTC+1 local noon -> @500", internet_time_beats_from_local(12, 0, 0, 60), 500);

    /* At UTC+00:00, BMT is local+1h: local 23:00:00 -> BMT midnight @000. */
    expect_eq_u16("UTC+0 local 23:00 -> @000", internet_time_beats_from_local(23, 0, 0, 0), 0);

    /* At UTC+05:30, local 04:30 == UTC 23:00 == BMT midnight. */
    expect_eq_u16(
        "UTC+5:30 local 04:30 -> @000", internet_time_beats_from_local(4, 30, 0, 5 * 60 + 30), 0);

    /* At UTC-08:00, local 15:00 == UTC 23:00 == BMT midnight. */
    expect_eq_u16(
        "UTC-8 local 15:00 -> @000", internet_time_beats_from_local(15, 0, 0, -8 * 60), 0);

    /* Negative wrap: UTC+14 local 00:00 -> UTC previous day 10:00 -> BMT 11:00. */
    expect_eq_u16(
        "UTC+14 local midnight wrap",
        internet_time_beats_from_local(0, 0, 0, 14 * 60),
        internet_time_beats_from_bmt_seconds(11 * 3600));

    /* Positive wrap: UTC-12 local 23:00 -> UTC next day 11:00 -> BMT 12:00 @500. */
    expect_eq_u16(
        "UTC-12 local 23:00 wrap", internet_time_beats_from_local(23, 0, 0, -12 * 60), 500);
}

int main(void) {
    test_offset_validation();
    test_bmt_boundaries();
    test_local_conversion_fixtures();

    if(failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    puts("ok");
    return 0;
}
