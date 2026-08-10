#include "dcf77_time.h"

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

static void test_cet_noon_to_utc_and_local(void) {
    Dcf77CivilTime civil = {
        .minute = 0,
        .hour = 12,
        .day = 10,
        .month = 8,
        .year = 2026,
        .weekday = 1,
        .cest = false,
    };
    Dcf77DateTime utc_as_local0;
    expect_true(
        "CET 12:00 -> offset 0 (UTC fields)", dcf77_civil_to_local(&civil, 0, &utc_as_local0));
    expect_eq_u8("CET noon UTC hour", utc_as_local0.hour, 11);
    expect_eq_u8("CET noon UTC minute", utc_as_local0.minute, 0);

    Dcf77DateTime local_plus120;
    expect_true("CET 12:00 -> offset +120", dcf77_civil_to_local(&civil, 120, &local_plus120));
    expect_eq_u8("CET noon local+120 hour", local_plus120.hour, 13);
}

static void test_cest_noon_to_utc_and_local(void) {
    Dcf77CivilTime civil = {
        .minute = 0,
        .hour = 12,
        .day = 10,
        .month = 8,
        .year = 2026,
        .weekday = 1,
        .cest = true,
    };
    Dcf77DateTime utc_as_local0;
    expect_true("CEST 12:00 -> offset 0", dcf77_civil_to_local(&civil, 0, &utc_as_local0));
    expect_eq_u8("CEST noon UTC hour", utc_as_local0.hour, 10);

    Dcf77DateTime local_plus120;
    expect_true("CEST 12:00 -> offset +120", dcf77_civil_to_local(&civil, 120, &local_plus120));
    expect_eq_u8("CEST noon local+120 hour", local_plus120.hour, 12);
    expect_true("weekday populated", local_plus120.weekday >= 1 && local_plus120.weekday <= 7);
    expect_true("year populated", local_plus120.year == 2026);
}

int test_dcf77_time_run(void) {
    failures = 0;
    test_cet_noon_to_utc_and_local();
    test_cest_noon_to_utc_and_local();
    return failures;
}

int test_dcf77_auto_sync_run(void) {
    failures = 0;
    const uint32_t interval = 12u * 3600u;
    expect_true("disabled", !dcf77_should_auto_sync(1000, 0, false, interval));
    expect_true("never synced", dcf77_should_auto_sync(1000, 0, true, interval));
    expect_true("age 11h", !dcf77_should_auto_sync(11u * 3600u, 0 + 1, true, interval));
    /* now=13h, last=1s → age ~13h */
    expect_true("age 13h", dcf77_should_auto_sync(13u * 3600u + 1u, 1u, true, interval));
    return failures;
}
