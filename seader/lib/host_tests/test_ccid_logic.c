#include <stdint.h>

#include "ccid_logic.h"
#include "munit.h"

static MunitResult test_sequence_advance_wraps(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says bSeq "is a monotonically increasing by one counter" and "rolls over to 00h after FFh." */
    uint8_t seq = 0;
    munit_assert_uint8(seader_ccid_sequence_advance(&seq), ==, 0);
    munit_assert_uint8(seq, ==, 1);

    seq = 254;
    munit_assert_uint8(seader_ccid_sequence_advance(&seq), ==, 254);
    munit_assert_uint8(seq, ==, 255);

    seq = 255;
    munit_assert_uint8(seader_ccid_sequence_advance(&seq), ==, 255);
    munit_assert_uint8(seq, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_payload_fits_buffer(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says XfrBlock "should never exceed the dwMaxCCIDMessageLength-10" because the header is 10 bytes. */
    munit_assert_true(seader_ccid_payload_fits_frame(10, 300, 12));
    munit_assert_false(seader_ccid_payload_fits_frame(289, 300, 12));
    munit_assert_true(seader_ccid_payload_fits_frame(288, 300, 12));
    return MUNIT_OK;
}

static MunitResult test_data_in_scratchpad(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says the 10-byte header provides "a constant offset at which message data begins across all messages." */
    uint8_t tx[64] = {0};
    munit_assert_true(seader_ccid_data_in_scratchpad(tx, sizeof(tx), 12, tx + 12, 1));
    munit_assert_true(seader_ccid_data_in_scratchpad(tx, sizeof(tx), 12, tx + 63, 1));
    munit_assert_false(seader_ccid_data_in_scratchpad(tx, sizeof(tx), 12, tx + 11, 1));
    munit_assert_false(seader_ccid_data_in_scratchpad(tx, sizeof(tx), 12, tx + 60, 8));
    return MUNIT_OK;
}

static MunitResult test_response_seq_match(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says responses use "the exact same sequence number contained in the command". */
    munit_assert_true(seader_ccid_response_matches_pending(false, 0x00, 0xff));
    munit_assert_true(seader_ccid_response_matches_pending(true, 0x12, 0x12));
    munit_assert_false(seader_ccid_response_matches_pending(true, 0x12, 0x13));
    return MUNIT_OK;
}

static MunitResult test_status_decode_ok(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says bmCommandStatus 0 means "Processed without error". */
    SeaderCcidStatus status_ok = seader_ccid_decode_status(0x00);
    munit_assert_uint8(status_ok.icc_status, ==, 0);
    munit_assert_int(status_ok.command_status, ==, SeaderCcidDecodedCommandStatusProcessed);
    return MUNIT_OK;
}

static MunitResult test_status_decode_failed(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says bmCommandStatus 1 means "Failed" and the slot error register carries the error code. */
    SeaderCcidStatus status_fail = seader_ccid_decode_status(0x41);
    munit_assert_uint8(status_fail.icc_status, ==, 1);
    munit_assert_int(status_fail.command_status, ==, SeaderCcidDecodedCommandStatusFailed);
    return MUNIT_OK;
}

static MunitResult test_status_decode_time_extension(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says bmCommandStatus 2 means "Time Extension is requested". */
    SeaderCcidStatus status_wait = seader_ccid_decode_status(0x80);
    munit_assert_uint8(status_wait.icc_status, ==, 0);
    munit_assert_int(
        status_wait.command_status, ==, SeaderCcidDecodedCommandStatusTimeExtension);
    return MUNIT_OK;
}

static MunitResult test_find_start_skips_nak_triplet(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* Bridge framing invariant: skip the serial NAK triplet before looking for the next sync/control framed packet. */
    const uint8_t sync = 0x03;
    const uint8_t ctrl = 0x06;
    const uint8_t nak = 0x15;
    const uint8_t frame_stream[] = {
        0x03,
        0x15,
        0x16,
        0x99,
        0x03,
        0x06,
        0x80,
    };

    munit_assert_size(
        seader_ccid_find_frame_start(frame_stream, sizeof(frame_stream), sync, ctrl, nak), ==, 4);
    return MUNIT_OK;
}

static MunitResult test_find_start_noise_only(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* The bridge must not invent a CCID frame start when the sync/control header is incomplete. */
    const uint8_t sync = 0x03;
    const uint8_t ctrl = 0x06;
    const uint8_t nak = 0x15;
    const uint8_t noise_only[] = {0x01, 0x02, 0x03};

    munit_assert_size(
        seader_ccid_find_frame_start(noise_only, sizeof(noise_only), sync, ctrl, nak),
        ==,
        sizeof(noise_only));
    return MUNIT_OK;
}

static MunitResult test_pending_timeout_helper(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says when Time Extension is set the host should "wait for a new response with the same bSeq number". */
    munit_assert_false(seader_ccid_pending_timed_out(false, 100, 200, 50));
    munit_assert_false(seader_ccid_pending_timed_out(true, 0, 200, 50));
    munit_assert_false(seader_ccid_pending_timed_out(true, 100, 149, 50));
    munit_assert_false(seader_ccid_pending_timed_out(true, 100, 150, 50));
    munit_assert_true(seader_ccid_pending_timed_out(true, 100, 151, 50));
    munit_assert_false(seader_ccid_pending_timed_out(true, 100, 200, 0));
    return MUNIT_OK;
}

static MunitResult test_data_block_route(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* CCID says "bSlot identifies which ICC slot is being addressed"; protocol 00h is T=0 and 01h is T=1. */
    munit_assert_int(seader_ccid_route_data_block(true, 0, 0, 0), ==, SeaderCcidDataRouteSamT0);
    munit_assert_int(seader_ccid_route_data_block(true, 0, 0, 1), ==, SeaderCcidDataRouteSamT1);
    munit_assert_int(
        seader_ccid_route_data_block(false, 0, 0, 1), ==, SeaderCcidDataRouteAtrRecognition);
    munit_assert_int(
        seader_ccid_route_data_block(true, 0, 1, 1), ==, SeaderCcidDataRouteWrongSlotError);
    return MUNIT_OK;
}

static MunitTest test_ccid_cases[] = {
    {(char*)"/sequence/advance-wraps-through-ff",
     test_sequence_advance_wraps,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/frame/payload-fits-buffer",
     test_payload_fits_buffer,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/frame/data-in-scratchpad",
     test_data_in_scratchpad,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/pending/response-seq-match",
     test_response_seq_match,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/status/decode-ok", test_status_decode_ok, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/status/decode-failed",
     test_status_decode_failed,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/status/decode-time-extension",
     test_status_decode_time_extension,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/frame/find-start-skips-nak-triplet",
     test_find_start_skips_nak_triplet,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/frame/find-start-noise-only",
     test_find_start_noise_only,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/pending/timeout-helper",
     test_pending_timeout_helper,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/routing/data-block-route",
     test_data_block_route,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_ccid_logic_suite = {
    "",
    test_ccid_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
