/* Collecting an answer that arrives in more than one frame.
 *
 * A card that cannot fit its answer in one frame ends each piece with 61xx and
 * waits to be asked for the rest. The whole answer is one protected message,
 * so a piece means nothing on its own and they are collected before anything
 * is unwrapped.
 *
 * The NFC reader drives this in a loop and the BLE readers drive it from
 * notifications, so it is a step rather than a loop.
 */
#include "munit.h"
#include "test_helpers.h"

#include <seos_sio_collect.h>
#include <seos_sm_command.h>

#include <string.h>

#define BUFFER_CAPACITY 512

/* A frame of `len` payload bytes ending in the given status word. */
static size_t frame(uint8_t* out, size_t len, uint8_t sw1, uint8_t sw2, uint8_t seed) {
    for(size_t i = 0; i < len; i++)
        out[i] = (uint8_t)(seed + i);
    out[len] = sw1;
    out[len + 1] = sw2;
    return len + 2;
}

static MunitResult test_single_frame(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosSioCollector collector;
    BitBuffer* assembled = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* next = bit_buffer_alloc(BUFFER_CAPACITY);
    uint8_t sent[] = {0x0c, 0xcb, 0x3f, 0xff, 0x08, 0x11, 0x22, 0x00};

    seos_sio_collect_begin(&collector, assembled, sent, sizeof(sent));

    uint8_t rx[64];
    size_t rx_len = frame(rx, 20, 0x90, 0x00, 0x40);

    munit_assert_int(
        seos_sio_collect_step(&collector, rx, rx_len, next), ==, SeosSioCollectComplete);
    munit_assert_size(bit_buffer_get_size_bytes(assembled), ==, 20);
    munit_assert_memory_equal(20, bit_buffer_get_data(assembled), rx);

    bit_buffer_free(assembled);
    bit_buffer_free(next);
    return MUNIT_OK;
}

/* Three pieces, the first two saying more is coming. */
static MunitResult test_chained_frames(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosSioCollector collector;
    BitBuffer* assembled = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* next = bit_buffer_alloc(BUFFER_CAPACITY);
    uint8_t sent[] = {0x0c, 0xcb, 0x3f, 0xff, 0x08, 0x11, 0x22, 0x00};

    seos_sio_collect_begin(&collector, assembled, sent, sizeof(sent));

    uint8_t rx[64];
    size_t rx_len = frame(rx, 30, 0x61, 0x20, 0x00);
    munit_assert_int(seos_sio_collect_step(&collector, rx, rx_len, next), ==, SeosSioCollectSend);
    /* What it asks to send is a request for the rest. */
    munit_assert_size(bit_buffer_get_size_bytes(next), ==, SEOS_GET_RESPONSE_LEN);
    munit_assert_uint8(bit_buffer_get_data(next)[1], ==, 0xc0);
    munit_assert_uint8(bit_buffer_get_data(next)[SEOS_GET_RESPONSE_LEN - 1], ==, 0x20);

    bit_buffer_reset(next);
    rx_len = frame(rx, 30, 0x61, 0x10, 0x30);
    munit_assert_int(seos_sio_collect_step(&collector, rx, rx_len, next), ==, SeosSioCollectSend);

    bit_buffer_reset(next);
    rx_len = frame(rx, 10, 0x90, 0x00, 0x60);
    munit_assert_int(
        seos_sio_collect_step(&collector, rx, rx_len, next), ==, SeosSioCollectComplete);

    /* Every piece, and only the pieces: no status words in the body. */
    munit_assert_size(bit_buffer_get_size_bytes(assembled), ==, 70);
    const uint8_t* body = bit_buffer_get_data(assembled);
    for(size_t i = 0; i < 30; i++)
        munit_assert_uint8(body[i], ==, (uint8_t)(0x00 + i));
    for(size_t i = 0; i < 30; i++)
        munit_assert_uint8(body[30 + i], ==, (uint8_t)(0x30 + i));
    for(size_t i = 0; i < 10; i++)
        munit_assert_uint8(body[60 + i], ==, (uint8_t)(0x60 + i));

    bit_buffer_free(assembled);
    bit_buffer_free(next);
    return MUNIT_OK;
}

/* A card asking for a different length gets the same command again, not a
 * freshly wrapped one: wrapping steps the sequence counter and the card would
 * then refuse the result. */
static MunitResult test_resend_keeps_the_command(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosSioCollector collector;
    BitBuffer* assembled = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* next = bit_buffer_alloc(BUFFER_CAPACITY);
    uint8_t sent[] = {0x0c, 0xcb, 0x3f, 0xff, 0x08, 0x11, 0x22, 0x00};

    seos_sio_collect_begin(&collector, assembled, sent, sizeof(sent));

    uint8_t rx[8];
    size_t rx_len = frame(rx, 0, 0x6c, 0x40, 0x00);
    munit_assert_int(seos_sio_collect_step(&collector, rx, rx_len, next), ==, SeosSioCollectSend);

    /* The same bytes, with the corrected length in the last one. */
    munit_assert_size(bit_buffer_get_size_bytes(next), ==, sizeof(sent));
    munit_assert_memory_equal(sizeof(sent) - 1, bit_buffer_get_data(next), sent);
    munit_assert_uint8(bit_buffer_get_data(next)[sizeof(sent) - 1], ==, 0x40);

    /* Nothing was collected from a correction. */
    munit_assert_size(bit_buffer_get_size_bytes(assembled), ==, 0);

    /* A card that keeps asking is not allowed to hold the reader. */
    bit_buffer_reset(next);
    munit_assert_int(
        seos_sio_collect_step(&collector, rx, rx_len, next), ==, SeosSioCollectFailed);

    bit_buffer_free(assembled);
    bit_buffer_free(next);
    return MUNIT_OK;
}

static MunitResult test_short_frame(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosSioCollector collector;
    BitBuffer* assembled = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* next = bit_buffer_alloc(BUFFER_CAPACITY);
    uint8_t sent[] = {0x0c, 0xcb, 0x3f, 0xff, 0x08, 0x11, 0x22, 0x00};

    seos_sio_collect_begin(&collector, assembled, sent, sizeof(sent));

    const uint8_t rx[] = {0x90};
    munit_assert_int(seos_sio_collect_step(&collector, rx, 0, next), ==, SeosSioCollectFailed);
    munit_assert_int(seos_sio_collect_step(&collector, rx, 1, next), ==, SeosSioCollectFailed);

    bit_buffer_free(assembled);
    bit_buffer_free(next);
    return MUNIT_OK;
}

static MunitResult test_error_status(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosSioCollector collector;
    BitBuffer* assembled = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* next = bit_buffer_alloc(BUFFER_CAPACITY);
    uint8_t sent[] = {0x0c, 0xcb, 0x3f, 0xff, 0x08, 0x11, 0x22, 0x00};

    seos_sio_collect_begin(&collector, assembled, sent, sizeof(sent));

    uint8_t rx[8];
    size_t rx_len = frame(rx, 0, 0x69, 0x88, 0x00);
    munit_assert_int(
        seos_sio_collect_step(&collector, rx, rx_len, next), ==, SeosSioCollectFailed);

    bit_buffer_free(assembled);
    bit_buffer_free(next);
    return MUNIT_OK;
}

/* A card that never says it is finished must not be able to hold the reader,
 * and must not be able to grow the buffer past what it holds. */
static MunitResult test_endless_chain(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosSioCollector collector;
    BitBuffer* assembled = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* next = bit_buffer_alloc(BUFFER_CAPACITY);
    uint8_t sent[] = {0x0c, 0xcb, 0x3f, 0xff, 0x08, 0x11, 0x22, 0x00};

    seos_sio_collect_begin(&collector, assembled, sent, sizeof(sent));

    uint8_t rx[80];
    size_t rx_len = frame(rx, 64, 0x61, 0x40, 0x00);

    SeosSioCollectResult result = SeosSioCollectSend;
    unsigned steps = 0;
    while(result == SeosSioCollectSend && steps < 100) {
        bit_buffer_reset(next);
        result = seos_sio_collect_step(&collector, rx, rx_len, next);
        steps++;
    }

    munit_assert_int(result, ==, SeosSioCollectFailed);
    munit_assert_uint(steps, <, 100);
    munit_assert_size(bit_buffer_get_size_bytes(assembled), <=, SEOS_SM_RESPONSE_MAX);

    bit_buffer_free(assembled);
    bit_buffer_free(next);
    return MUNIT_OK;
}

static MunitTest test_sio_collect_cases[] = {
    {(char*)"/single-frame", test_single_frame, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/chained", test_chained_frames, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/resend", test_resend_keeps_the_command, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/short-frame", test_short_frame, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/error-status", test_error_status, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/endless-chain", test_endless_chain, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_sio_collect_suite = {
    (char*)"/sio-collect",
    test_sio_collect_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
