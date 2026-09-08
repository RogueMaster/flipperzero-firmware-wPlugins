#include "bridge_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned frames;
    unsigned errors;
    FibParseError last_error;
    FibFrame last_frame;
} TestContext;

static void on_frame(const FibFrame* frame, void* context) {
    TestContext* test = context;
    ++test->frames;
    test->last_frame = *frame;
}

static void on_error(FibParseError error, void* context) {
    TestContext* test = context;
    ++test->errors;
    test->last_error = error;
}

static FibFrame make_ping(void) {
    FibFrame frame = {0};
    frame.major = FIB_PROTOCOL_MAJOR;
    frame.minor = FIB_PROTOCOL_MINOR;
    frame.type = FibMessagePing;
    frame.sequence = 1U;
    frame.payload_length = 8U;
    fib_write_u64_le(frame.payload, UINT64_C(0x0102030405060708));
    return frame;
}

static void test_crc_reference(void) {
    static const uint8_t value[] = "123456789";
    assert(fib_crc32(value, sizeof(value) - 1U) == 0xCBF43926U);
}

static void test_known_ping_vector(void) {
    static const uint8_t expected[] = {
        0x46, 0x49, 0x42, 0x50, 0x01, 0x00, 0x1C, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xBC, 0x31, 0x4C, 0x10,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xBE, 0xDD, 0xFB, 0x32,
    };
    const FibFrame frame = make_ping();
    uint8_t encoded[FIB_MAX_FRAME_SIZE];
    size_t written = 0U;
    assert(fib_frame_encode(&frame, encoded, sizeof(encoded), &written));
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void test_every_split_and_bytewise(void) {
    const FibFrame frame = make_ping();
    uint8_t encoded[FIB_MAX_FRAME_SIZE];
    size_t written = 0U;
    assert(fib_frame_encode(&frame, encoded, sizeof(encoded), &written));

    for(size_t split = 0U; split <= written; ++split) {
        TestContext test = {0};
        FibParser parser;
        fib_parser_init(&parser, on_frame, on_error, &test);
        fib_parser_feed(&parser, encoded, split);
        fib_parser_feed(&parser, encoded + split, written - split);
        assert(test.frames == 1U);
        assert(test.errors == 0U);
        assert(test.last_frame.type == FibMessagePing);
        assert(fib_read_u64_le(test.last_frame.payload) == UINT64_C(0x0102030405060708));
    }

    TestContext test = {0};
    FibParser parser;
    fib_parser_init(&parser, on_frame, on_error, &test);
    for(size_t index = 0U; index < written; ++index) {
        fib_parser_feed(&parser, encoded + index, 1U);
    }
    assert(test.frames == 1U);
    assert(test.errors == 0U);
}

static void test_concatenated_and_leading_garbage(void) {
    const FibFrame frame = make_ping();
    uint8_t encoded[FIB_MAX_FRAME_SIZE];
    size_t written = 0U;
    assert(fib_frame_encode(&frame, encoded, sizeof(encoded), &written));

    uint8_t stream[(FIB_MAX_FRAME_SIZE * 2U) + 7U];
    static const uint8_t garbage[] = {0x00, 'F', 'I', 'X', 0xFF, 'F', 'F'};
    memcpy(stream, garbage, sizeof(garbage));
    memcpy(stream + sizeof(garbage), encoded, written);
    memcpy(stream + sizeof(garbage) + written, encoded, written);

    TestContext test = {0};
    FibParser parser;
    fib_parser_init(&parser, on_frame, on_error, &test);
    fib_parser_feed(&parser, stream, sizeof(garbage) + written * 2U);
    assert(test.frames == 2U);
    assert(test.errors == 0U);
}

static void test_crc_rejection_and_recovery(void) {
    const FibFrame frame = make_ping();
    uint8_t encoded[FIB_MAX_FRAME_SIZE];
    size_t written = 0U;
    assert(fib_frame_encode(&frame, encoded, sizeof(encoded), &written));

    uint8_t stream[FIB_MAX_FRAME_SIZE * 2U];
    memcpy(stream, encoded, written);
    stream[FIB_HEADER_SIZE] ^= 1U;
    memcpy(stream + written, encoded, written);

    TestContext test = {0};
    FibParser parser;
    fib_parser_init(&parser, on_frame, on_error, &test);
    fib_parser_feed(&parser, stream, written * 2U);
    assert(test.errors == 1U);
    assert(test.last_error == FibParseErrorBadFrameCrc);
    assert(test.frames == 1U);
}

static void test_header_crc_and_oversize_rejection(void) {
    FibFrame frame = make_ping();
    uint8_t encoded[FIB_MAX_FRAME_SIZE];
    size_t written = 0U;
    assert(fib_frame_encode(&frame, encoded, sizeof(encoded), &written));

    encoded[12] ^= 1U;
    TestContext bad_header = {0};
    FibParser parser;
    fib_parser_init(&parser, on_frame, on_error, &bad_header);
    fib_parser_feed(&parser, encoded, written);
    assert(bad_header.frames == 0U);
    assert(bad_header.errors == 1U);
    assert(bad_header.last_error == FibParseErrorBadHeaderCrc);

    static const uint8_t oversize_header[] = {
        0x46, 0x49, 0x42, 0x50, 0x01, 0x00, 0x1C, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x58, 0xAA, 0xC0, 0x6E,
    };
    TestContext oversize = {0};
    fib_parser_init(&parser, on_frame, on_error, &oversize);
    fib_parser_feed(&parser, oversize_header, sizeof(oversize_header));
    assert(oversize.frames == 0U);
    assert(oversize.errors == 1U);
    assert(oversize.last_error == FibParseErrorPayloadTooLarge);
}

static void test_invalid_header_recovers_embedded_magic_suffix(void) {
    const FibFrame frame = make_ping();
    uint8_t encoded[FIB_MAX_FRAME_SIZE];
    size_t written = 0U;
    assert(fib_frame_encode(&frame, encoded, sizeof(encoded), &written));

    uint8_t stream[20U + FIB_MAX_FRAME_SIZE];
    memset(stream, 0, 20U);
    memcpy(stream, "FIBP", 4U);
    stream[4] = FIB_PROTOCOL_MAJOR;
    stream[5] = FIB_PROTOCOL_MINOR;
    stream[6] = FIB_HEADER_SIZE;
    stream[7] = FibMessagePing;
    memcpy(stream + 20U, encoded, written);

    TestContext test = {0};
    FibParser parser;
    fib_parser_init(&parser, on_frame, on_error, &test);
    fib_parser_feed(&parser, stream, 20U + written);
    assert(test.errors == 1U);
    assert(test.last_error == FibParseErrorBadHeaderCrc);
    assert(test.frames == 1U);
    assert(test.last_frame.type == FibMessagePing);
}

static void test_bad_frame_recovers_fully_embedded_frame(void) {
    const FibFrame ping = make_ping();
    uint8_t ping_encoded[FIB_MAX_FRAME_SIZE];
    size_t ping_written = 0U;
    assert(fib_frame_encode(&ping, ping_encoded, sizeof(ping_encoded), &ping_written));

    FibFrame outer = {0};
    outer.major = FIB_PROTOCOL_MAJOR;
    outer.minor = FIB_PROTOCOL_MINOR;
    outer.type = FibMessageResponseBodyChunk;
    outer.request_id = 7U;
    outer.payload_length = (uint32_t)(8U + ping_written);
    memset(outer.payload, 0xCC, 8U);
    memcpy(outer.payload + 8U, ping_encoded, ping_written);

    uint8_t stream[FIB_MAX_FRAME_SIZE];
    size_t stream_length = 0U;
    assert(fib_frame_encode(&outer, stream, sizeof(stream), &stream_length));
    stream[stream_length - 1U] ^= 0x80U;

    TestContext test = {0};
    FibParser parser;
    fib_parser_init(&parser, on_frame, on_error, &test);
    fib_parser_feed(&parser, stream, stream_length);
    assert(test.errors == 1U);
    assert(test.last_error == FibParseErrorBadFrameCrc);
    assert(test.frames == 1U);
    assert(test.last_frame.type == FibMessagePing);
    assert(fib_read_u64_le(test.last_frame.payload) == UINT64_C(0x0102030405060708));
}

static void test_encoder_bounds(void) {
    FibFrame frame = make_ping();
    uint8_t output[39];
    size_t written = 99U;
    assert(!fib_frame_encode(&frame, output, sizeof(output), &written));
    assert(written == 0U);
    frame.payload_length = FIB_MAX_FRAME_PAYLOAD + 1U;
    assert(!fib_frame_encode(&frame, output, sizeof(output), &written));
}

int main(void) {
    test_crc_reference();
    test_known_ping_vector();
    test_every_split_and_bytewise();
    test_concatenated_and_leading_garbage();
    test_crc_rejection_and_recovery();
    test_header_crc_and_oversize_rejection();
    test_invalid_header_recovers_embedded_magic_suffix();
    test_bad_frame_recovers_fully_embedded_frame();
    test_encoder_bounds();
    puts("bridge_protocol tests: PASS");
    return 0;
}
