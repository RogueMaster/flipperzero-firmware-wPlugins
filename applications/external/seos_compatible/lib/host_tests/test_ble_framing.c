/* Splitting a message across BLE frames, and putting it back together.
 *
 * Four transports each carried a copy of this, two of which leaked their
 * working buffer on the error path. Testing the one copy is what makes them
 * agree.
 */
#include "munit.h"
#include "test_helpers.h"

#include <seos_ble_framing.h>

#define RX_CAPACITY 512

typedef struct {
    uint8_t frames[16][1 + BLE_CHUNK_SIZE];
    size_t lengths[16];
    size_t count;
    size_t stop_after; /* refuse the chunk at this index, to test the stop */
} ChunkLog;

static bool record_chunk(void* context, const uint8_t* chunk, size_t chunk_len) {
    ChunkLog* log = context;
    munit_assert_size(log->count, <, 16);
    munit_assert_size(chunk_len, <=, 1 + BLE_CHUNK_SIZE);
    memcpy(log->frames[log->count], chunk, chunk_len);
    log->lengths[log->count] = chunk_len;
    log->count++;
    return log->count < log->stop_after;
}

/* Whatever goes out in chunks must come back whole. */
static void round_trip(size_t size) {
    uint8_t message[300];
    for(size_t i = 0; i < size; i++)
        message[i] = (uint8_t)(i * 5 + 3);

    ChunkLog log = {.stop_after = 16};
    munit_assert_true(seos_ble_chunk(message, size, record_chunk, &log));
    munit_assert_size(log.count, ==, (size + BLE_CHUNK_SIZE - 1) / BLE_CHUNK_SIZE);

    BitBuffer* rx = bit_buffer_alloc(RX_CAPACITY);
    for(size_t i = 0; i < log.count; i++) {
        SeosBleFrameResult result = seos_ble_reassemble(rx, log.frames[i], log.lengths[i]);
        if(i + 1 < log.count) {
            munit_assert_int(result, ==, SeosBleFrameIncomplete);
        } else {
            munit_assert_int(result, ==, SeosBleFrameComplete);
        }
    }

    munit_assert_size(bit_buffer_get_size_bytes(rx), ==, size);
    munit_assert_memory_equal(size, bit_buffer_get_data(rx), message);
    bit_buffer_free(rx);
}

static MunitResult test_round_trip(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    for(size_t size = 1; size <= 200; size++) {
        round_trip(size);
    }
    return MUNIT_OK;
}

/* The first chunk says so, the last says so, and the low nibble counts down. */
static MunitResult test_chunk_flags(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t message[BLE_CHUNK_SIZE * 3];
    memset(message, 0xa5, sizeof(message));

    ChunkLog log = {.stop_after = 16};
    seos_ble_chunk(message, sizeof(message), record_chunk, &log);
    munit_assert_size(log.count, ==, 3);

    munit_assert_true((log.frames[0][0] & BLE_FLAG_SOM) != 0);
    munit_assert_false((log.frames[0][0] & BLE_FLAG_EOM) != 0);
    munit_assert_uint8(log.frames[0][0] & 0x0f, ==, 2);
    munit_assert_uint8(log.frames[1][0] & 0x0f, ==, 1);
    munit_assert_true((log.frames[2][0] & BLE_FLAG_EOM) != 0);
    munit_assert_uint8(log.frames[2][0] & 0x0f, ==, 0);
    return MUNIT_OK;
}

/* A message of exactly one chunk is both the start and the end of itself. */
static MunitResult test_single_chunk(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t message[] = {0x01, 0x02, 0x03};
    ChunkLog log = {.stop_after = 16};
    seos_ble_chunk(message, sizeof(message), record_chunk, &log);

    munit_assert_size(log.count, ==, 1);
    munit_assert_true((log.frames[0][0] & BLE_FLAG_SOM) != 0);
    munit_assert_true((log.frames[0][0] & BLE_FLAG_EOM) != 0);
    return MUNIT_OK;
}

static MunitResult test_empty_sends_nothing(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    ChunkLog log = {.stop_after = 16};
    seos_ble_chunk((const uint8_t*)"", 0, record_chunk, &log);
    munit_assert_size(log.count, ==, 0);
    return MUNIT_OK;
}

/* A continuation with nothing started is not the start of a message. */
static MunitResult test_rejects_orphan_continuation(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    BitBuffer* rx = bit_buffer_alloc(RX_CAPACITY);
    uint8_t frame[] = {0x00, 0xaa, 0xbb};

    munit_assert_int(seos_ble_reassemble(rx, frame, sizeof(frame)), ==, SeosBleFrameError);
    munit_assert_size(bit_buffer_get_size_bytes(rx), ==, 0);

    bit_buffer_free(rx);
    return MUNIT_OK;
}

/* A peer reporting an error is not a message. */
static MunitResult test_error_frame(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    BitBuffer* rx = bit_buffer_alloc(RX_CAPACITY);
    uint8_t frame[] = {BLE_FLAG_SOM | BLE_FLAG_ERR, 0x6a, 0x82};

    munit_assert_int(seos_ble_reassemble(rx, frame, sizeof(frame)), ==, SeosBleFrameError);
    bit_buffer_free(rx);
    return MUNIT_OK;
}

static MunitResult test_empty_frame(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    BitBuffer* rx = bit_buffer_alloc(RX_CAPACITY);
    munit_assert_int(seos_ble_reassemble(rx, (const uint8_t*)"", 0), ==, SeosBleFrameError);
    bit_buffer_free(rx);
    return MUNIT_OK;
}

/* A message longer than the buffer must be refused, not written past it. */
static MunitResult test_overlong_message(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    BitBuffer* rx = bit_buffer_alloc(BLE_CHUNK_SIZE * 2);
    uint8_t message[BLE_CHUNK_SIZE * 4];
    memset(message, 0x5a, sizeof(message));

    ChunkLog log = {.stop_after = 16};
    seos_ble_chunk(message, sizeof(message), record_chunk, &log);

    bool refused = false;
    for(size_t i = 0; i < log.count; i++) {
        if(seos_ble_reassemble(rx, log.frames[i], log.lengths[i]) == SeosBleFrameError) {
            refused = true;
            break;
        }
    }
    munit_assert_true(refused);

    bit_buffer_free(rx);
    return MUNIT_OK;
}

/* A restart mid-message throws away what came before it. */
static MunitResult test_restart_discards(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    BitBuffer* rx = bit_buffer_alloc(RX_CAPACITY);

    uint8_t first[] = {BLE_FLAG_SOM, 0x11, 0x22};
    uint8_t restart[] = {BLE_FLAG_SOM | BLE_FLAG_EOM, 0x33};

    munit_assert_int(seos_ble_reassemble(rx, first, sizeof(first)), ==, SeosBleFrameIncomplete);
    munit_assert_int(seos_ble_reassemble(rx, restart, sizeof(restart)), ==, SeosBleFrameComplete);

    munit_assert_size(bit_buffer_get_size_bytes(rx), ==, 1);
    munit_assert_uint8(bit_buffer_get_byte(rx, 0), ==, 0x33);

    bit_buffer_free(rx);
    return MUNIT_OK;
}

/* A transport that refuses a chunk stops the rest being sent. */
static MunitResult test_send_failure_stops(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t message[BLE_CHUNK_SIZE * 4];
    memset(message, 0x33, sizeof(message));

    ChunkLog log = {.stop_after = 2};
    munit_assert_false(seos_ble_chunk(message, sizeof(message), record_chunk, &log));
    munit_assert_size(log.count, ==, 2);
    return MUNIT_OK;
}

static MunitTest test_ble_framing_cases[] = {
    {(char*)"/round-trip", test_round_trip, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/chunk/flags", test_chunk_flags, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/chunk/single", test_single_chunk, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/chunk/send-failure",
     test_send_failure_stops,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/chunk/empty", test_empty_sends_nothing, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reassemble/orphan",
     test_rejects_orphan_continuation,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reassemble/error", test_error_frame, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reassemble/empty", test_empty_frame, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reassemble/overlong", test_overlong_message, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reassemble/restart", test_restart_discards, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_ble_framing_suite = {
    (char*)"/ble-framing",
    test_ble_framing_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
