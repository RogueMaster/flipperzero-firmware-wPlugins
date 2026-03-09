#include <stdint.h>

#include "lrc.h"
#include "munit.h"

static MunitResult test_calc_lrc(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says "exclusive-oring all the bytes of the block from NAD to LRC inclusive shall give '00'." */
    uint8_t bytes[] = {0x03, 0x06, 0x62, 0x00};
    munit_assert_uint8(
        seader_calc_lrc(bytes, sizeof(bytes)), ==, (uint8_t)(0x03 ^ 0x06 ^ 0x62 ^ 0x00));
    return MUNIT_OK;
}

static MunitResult test_add_and_validate_lrc(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says "Any other value is invalid", so flipping one byte must break validation. */
    uint8_t frame[8] = {0x03, 0x06, 0x62, 0x00, 0x01, 0x02, 0x00, 0x00};
    size_t len = seader_add_lrc(frame, 6);
    munit_assert_size(len, ==, 7);
    munit_assert_true(seader_validate_lrc(frame, len));

    frame[2] ^= 0x80;
    munit_assert_false(seader_validate_lrc(frame, len));
    return MUNIT_OK;
}

static MunitResult test_validate_nak_triplet(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* Bridge framing invariant: SYNC, NAK, and XOR checksum form the transport-side NAK triplet. */
    uint8_t nak_triplet[] = {0x03, 0x15, 0x16};
    munit_assert_true(seader_validate_lrc(nak_triplet, 3));
    return MUNIT_OK;
}

static MunitTest test_lrc_cases[] = {
    {(char*)"/calc", test_calc_lrc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/roundtrip", test_add_and_validate_lrc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/nak-triplet", test_validate_nak_triplet, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_lrc_suite = {
    "",
    test_lrc_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
