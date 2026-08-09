#include "clock_model.h"
#include "internet_time.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(const char* name, bool cond) {
    if(!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void expect_str(const char* name, const char* got, const char* want) {
    if(got == NULL || want == NULL || strcmp(got, want) != 0) {
        fprintf(
            stderr,
            "FAIL: %s (got \"%s\", want \"%s\")\n",
            name,
            got ? got : "(null)",
            want ? want : "(null)");
        failures++;
    }
}

static void expect_eq_u16(const char* name, uint16_t got, uint16_t want) {
    if(got != want) {
        fprintf(stderr, "FAIL: %s (got %u, want %u)\n", name, got, want);
        failures++;
    }
}

static void test_format_beats(void) {
    char buf[8];
    expect_true("beats @000", clock_model_format_beats(0, buf, sizeof(buf)));
    expect_str("beats text @000", buf, "@000");
    expect_true("beats @500", clock_model_format_beats(500, buf, sizeof(buf)));
    expect_str("beats text @500", buf, "@500");
    expect_true("beats @999", clock_model_format_beats(999, buf, sizeof(buf)));
    expect_str("beats text @999", buf, "@999");
    expect_true("beats buf too small", !clock_model_format_beats(1, buf, 4));
}

static void test_format_local_24(void) {
    char buf[16];
    expect_true("24h format", clock_model_format_local_time(0, 0, 0, true, buf, sizeof(buf)));
    expect_str("24h midnight", buf, "00:00:00");
    expect_true("24h afternoon", clock_model_format_local_time(15, 4, 9, true, buf, sizeof(buf)));
    expect_str("24h 15:04:09", buf, "15:04:09");
}

static void test_format_local_12(void) {
    char buf[16];
    expect_true("12h midnight", clock_model_format_local_time(0, 0, 0, false, buf, sizeof(buf)));
    expect_str("12h midnight text", buf, "12:00:00 AM");
    expect_true("12h noon", clock_model_format_local_time(12, 0, 0, false, buf, sizeof(buf)));
    expect_str("12h noon text", buf, "12:00:00 PM");
    expect_true("12h morning", clock_model_format_local_time(9, 5, 6, false, buf, sizeof(buf)));
    expect_str("12h 09:05:06 AM", buf, "09:05:06 AM");
    expect_true("12h evening", clock_model_format_local_time(23, 59, 59, false, buf, sizeof(buf)));
    expect_str("12h 11:59:59 PM", buf, "11:59:59 PM");
    expect_true("12h buf too small", !clock_model_format_local_time(1, 2, 3, false, buf, 11));
}

static void test_snapshot(void) {
    ClockModelInput in = {
        .hour = 12,
        .minute = 0,
        .second = 0,
        .utc_offset_minutes = 60,
        .hour_format_24 = true,
    };
    ClockModelSnapshot snap;
    clock_model_build_snapshot(&in, &snap);
    expect_eq_u16("snapshot beats noon BMT", snap.beats, 500);
    expect_str("snapshot beats text", snap.beats_text, "@500");
    expect_str("snapshot local 24h", snap.local_time_text, "12:00:00");

    in.hour_format_24 = false;
    in.hour = 0;
    in.minute = 0;
    in.second = 0;
    clock_model_build_snapshot(&in, &snap);
    expect_eq_u16("snapshot beats midnight BMT", snap.beats, 0);
    expect_str("snapshot local 12h", snap.local_time_text, "12:00:00 AM");
}

int test_clock_model_run(void) {
    failures = 0;
    test_format_beats();
    test_format_local_24();
    test_format_local_12();
    test_snapshot();
    return failures;
}
