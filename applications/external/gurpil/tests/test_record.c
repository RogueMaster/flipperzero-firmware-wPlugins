#include "include/domain/record.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_round_trip_zero(void) {
    uint8_t buf[RECORD_BYTES];

    size_t written = record_serialize(0, buf, sizeof(buf));
    assert(written == RECORD_BYTES);

    int32_t parsed = record_parse(buf, sizeof(buf));
    assert(parsed == 0);
}

static void test_round_trip_positive(void) {
    uint8_t buf[RECORD_BYTES];

    int32_t original = 12345;
    size_t written = record_serialize(original, buf, sizeof(buf));
    assert(written == RECORD_BYTES);

    int32_t parsed = record_parse(buf, sizeof(buf));
    assert(parsed == original);
}

static void test_round_trip_large(void) {
    uint8_t buf[RECORD_BYTES];

    int32_t original = 2147483647; // INT32_MAX
    size_t written = record_serialize(original, buf, sizeof(buf));
    assert(written == RECORD_BYTES);

    int32_t parsed = record_parse(buf, sizeof(buf));
    assert(parsed == original);
}

static void test_short_buffer_serialize_returns_zero(void) {
    uint8_t buf[RECORD_BYTES - 1];

    size_t written = record_serialize(100, buf, sizeof(buf));
    assert(written == 0);
}

static void test_short_buffer_serialize_writes_nothing(void) {
    // Verify that a short buffer is not touched at all.
    uint8_t buf[RECORD_BYTES - 1];
    memset(buf, 0xFF, sizeof(buf));
    uint8_t before[RECORD_BYTES - 1];
    memcpy(before, buf, sizeof(buf));

    record_serialize(100, buf, sizeof(buf));

    // Buffer must remain unchanged.
    assert(memcmp(buf, before, sizeof(buf)) == 0);
}

static void test_short_buffer_parse_returns_zero(void) {
    uint8_t buf[RECORD_BYTES - 1];
    memset(buf, 0x42, sizeof(buf)); // Fill with plausible data.

    int32_t parsed = record_parse(buf, sizeof(buf));
    assert(parsed == 0);
}

static void test_corrupt_magic_returns_zero(void) {
    uint8_t buf[RECORD_BYTES];
    memset(buf, 0x00, sizeof(buf));

    // Set wrong magic (anything but 0x42).
    buf[0] = 0x41;
    buf[1] = 0x01;
    buf[4] = 100;

    int32_t parsed = record_parse(buf, sizeof(buf));
    assert(parsed == 0);
}

static void test_corrupt_version_returns_zero(void) {
    uint8_t buf[RECORD_BYTES];
    memset(buf, 0x00, sizeof(buf));

    // Set correct magic but wrong version.
    buf[0] = 0x42;
    buf[1] = 0x02;
    buf[4] = 100;

    int32_t parsed = record_parse(buf, sizeof(buf));
    assert(parsed == 0);
}

static void test_null_buffer_parse_returns_zero(void) {
    int32_t parsed = record_parse(NULL, RECORD_BYTES);
    assert(parsed == 0);
}

static void test_parse_does_not_read_past_record_bytes(void) {
    // Create a valid RECORD_BYTES buffer followed by garbage.
    uint8_t buf[RECORD_BYTES + 100];
    memset(buf, 0xFF, sizeof(buf));

    // Write a valid record.
    record_serialize(777, buf, RECORD_BYTES);

    // Fill the rest with garbage (should not be read).
    memset(buf + RECORD_BYTES, 0x42, sizeof(buf) - RECORD_BYTES);

    // Parse with only RECORD_BYTES length hint; should not crash or read garbage.
    int32_t parsed = record_parse(buf, RECORD_BYTES);
    assert(parsed == 777);
}

static void test_update_keeps_max(void) {
    int32_t prev = 100;
    int32_t result = record_update(prev, 200);
    assert(result == 200);

    result = record_update(200, 100);
    assert(result == 200);

    result = record_update(100, 100);
    assert(result == 100);
}

static void test_update_clamps_negative_distance(void) {
    int32_t prev = 100;
    int32_t result = record_update(prev, -50);
    assert(result == prev); // Negative never lowers the record.

    prev = 0;
    result = record_update(prev, -1);
    assert(result == 0);
}

static void test_update_negative_to_zero_stays_zero(void) {
    int32_t prev = 0;
    int32_t result = record_update(prev, -100);
    assert(result == 0);
}

static void test_update_larger_increases_record(void) {
    int32_t prev = 0;
    int32_t result = record_update(prev, 1);
    assert(result == 1);

    result = record_update(1, 2);
    assert(result == 2);

    result = record_update(2147483646, 2147483647);
    assert(result == 2147483647);
}

int main(void) {
    test_round_trip_zero();
    printf("test_round_trip_zero: PASS\n");

    test_round_trip_positive();
    printf("test_round_trip_positive: PASS\n");

    test_round_trip_large();
    printf("test_round_trip_large: PASS\n");

    test_short_buffer_serialize_returns_zero();
    printf("test_short_buffer_serialize_returns_zero: PASS\n");

    test_short_buffer_serialize_writes_nothing();
    printf("test_short_buffer_serialize_writes_nothing: PASS\n");

    test_short_buffer_parse_returns_zero();
    printf("test_short_buffer_parse_returns_zero: PASS\n");

    test_corrupt_magic_returns_zero();
    printf("test_corrupt_magic_returns_zero: PASS\n");

    test_corrupt_version_returns_zero();
    printf("test_corrupt_version_returns_zero: PASS\n");

    test_null_buffer_parse_returns_zero();
    printf("test_null_buffer_parse_returns_zero: PASS\n");

    test_parse_does_not_read_past_record_bytes();
    printf("test_parse_does_not_read_past_record_bytes: PASS\n");

    test_update_keeps_max();
    printf("test_update_keeps_max: PASS\n");

    test_update_clamps_negative_distance();
    printf("test_update_clamps_negative_distance: PASS\n");

    test_update_negative_to_zero_stays_zero();
    printf("test_update_negative_to_zero_stays_zero: PASS\n");

    test_update_larger_increases_record();
    printf("test_update_larger_increases_record: PASS\n");

    return 0;
}
