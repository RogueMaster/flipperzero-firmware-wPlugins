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

static void test_is_valid_true_for_serialized(void) {
    uint8_t buf[RECORD_BYTES];
    record_serialize(42, buf, sizeof(buf));
    assert(record_is_valid(buf, sizeof(buf)));
}

static void test_is_valid_false_for_short_buffer(void) {
    uint8_t buf[RECORD_BYTES];
    record_serialize(42, buf, sizeof(buf));
    assert(!record_is_valid(buf, RECORD_BYTES - 1));
}

static void test_is_valid_false_for_bad_magic(void) {
    uint8_t buf[RECORD_BYTES];
    record_serialize(42, buf, sizeof(buf));
    buf[0] = 0x00;
    assert(!record_is_valid(buf, sizeof(buf)));
}

static void test_is_valid_false_for_bad_version(void) {
    uint8_t buf[RECORD_BYTES];
    record_serialize(42, buf, sizeof(buf));
    buf[1] = 0x02;
    assert(!record_is_valid(buf, sizeof(buf)));
}

static void test_is_valid_false_for_null(void) {
    assert(!record_is_valid(NULL, RECORD_BYTES));
}

// Missing save data: no candidate slot has been taken yet, so the first quarantine goes to
// slot 0 and nothing pre-empts a future save.
static void test_backup_slot_picks_first_free(void) {
    bool none_taken[RECORD_BACKUP_SLOTS] = {0};
    assert(record_backup_slot(none_taken) == 0);
}

// A second corruption, arriving while the first backup still exists, must move to the next
// free slot instead of overwriting it (keeps the first copy).
static void test_backup_slot_skips_taken_slots(void) {
    bool taken[RECORD_BACKUP_SLOTS] = {0};
    int first = record_backup_slot(taken);
    assert(first == 0);
    taken[first] = true;

    int second = record_backup_slot(taken);
    assert(second == 1);
}

static void test_backup_slot_all_taken_returns_negative(void) {
    bool all_taken[RECORD_BACKUP_SLOTS];
    for(int i = 0; i < RECORD_BACKUP_SLOTS; i++) {
        all_taken[i] = true;
    }
    assert(record_backup_slot(all_taken) == -1);
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

    test_is_valid_true_for_serialized();
    printf("test_is_valid_true_for_serialized: PASS\n");

    test_is_valid_false_for_short_buffer();
    printf("test_is_valid_false_for_short_buffer: PASS\n");

    test_is_valid_false_for_bad_magic();
    printf("test_is_valid_false_for_bad_magic: PASS\n");

    test_is_valid_false_for_bad_version();
    printf("test_is_valid_false_for_bad_version: PASS\n");

    test_is_valid_false_for_null();
    printf("test_is_valid_false_for_null: PASS\n");

    test_backup_slot_picks_first_free();
    printf("test_backup_slot_picks_first_free: PASS\n");

    test_backup_slot_skips_taken_slots();
    printf("test_backup_slot_skips_taken_slots: PASS\n");

    test_backup_slot_all_taken_returns_negative();
    printf("test_backup_slot_all_taken_returns_negative: PASS\n");

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
