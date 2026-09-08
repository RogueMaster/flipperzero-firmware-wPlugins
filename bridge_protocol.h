#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIB_MAGIC_SIZE        4U
#define FIB_HEADER_SIZE       28U
#define FIB_HEADER_CRC_OFFSET 24U
#define FIB_FRAME_CRC_SIZE    4U
#define FIB_MAX_FRAME_SIZE    (FIB_HEADER_SIZE + FIB_MAX_FRAME_PAYLOAD + FIB_FRAME_CRC_SIZE)

typedef enum {
    FibMessageHello = 0x01,
    FibMessageHelloAck = 0x02,
    FibMessagePermissionStatus = 0x03,
    FibMessagePermissionRequired = 0x04,
    FibMessageRequestStart = 0x10,
    FibMessageRequestHeader = 0x11,
    FibMessageRequestBodyChunk = 0x12,
    FibMessageRequestEnd = 0x13,
    FibMessageResponseStart = 0x20,
    FibMessageResponseHeader = 0x21,
    FibMessageResponseBodyChunk = 0x22,
    FibMessageResponseEnd = 0x23,
    FibMessageCancel = 0x30,
    FibMessagePing = 0x31,
    FibMessagePong = 0x32,
    FibMessageError = 0x7E,
    FibMessageDisconnect = 0x7F,
} FibMessageType;

typedef enum {
    FibFlagAckRequired = 0x0001,
    FibFlagFinal = 0x0002,
    FibFlagTruncated = 0x0004,
    FibFlagRetryable = 0x0008,
} FibFrameFlags;

typedef enum {
    FibErrorMalformedFrame = 0x0001,
    FibErrorBadHeaderCrc = 0x0002,
    FibErrorBadFrameCrc = 0x0003,
    FibErrorPayloadTooLarge = 0x0004,
    FibErrorUnsupportedVersion = 0x0005,
    FibErrorUnsupportedMessage = 0x0006,
    FibErrorDuplicateSequence = 0x0007,
    FibErrorSequenceGap = 0x0008,
    FibErrorInvalidState = 0x0009,
    FibErrorDuplicateRequestId = 0x000A,
    FibErrorPermissionDenied = 0x000B,
    FibErrorInvalidRequest = 0x000C,
    FibErrorSecurityBlocked = 0x000D,
    FibErrorTimeout = 0x000E,
    FibErrorCancelled = 0x000F,
    FibErrorResponseTooLarge = 0x0010,
    FibErrorRxOverflow = 0x0011,
    FibErrorInternal = 0x0012,
    FibErrorTransportLost = 0x0013,
    FibErrorNetworkFailure = 0x0014,
} FibErrorCode;

typedef enum {
    FibParseErrorBadHeaderCrc,
    FibParseErrorPayloadTooLarge,
    FibParseErrorBadFrameCrc,
    FibParseErrorInvalidHeader,
} FibParseError;

typedef struct {
    uint8_t major;
    uint8_t minor;
    FibMessageType type;
    uint16_t flags;
    uint32_t request_id;
    uint32_t sequence;
    uint32_t payload_length;
    uint8_t payload[FIB_MAX_FRAME_PAYLOAD];
} FibFrame;

typedef void (*FibFrameCallback)(const FibFrame* frame, void* context);
typedef void (*FibParseErrorCallback)(FibParseError error, void* context);

typedef struct {
    uint8_t frame_bytes[FIB_MAX_FRAME_SIZE];
    size_t used;
    size_t expected;
    uint8_t magic_matched;
    bool reading_frame;
    FibFrame frame;
    FibFrameCallback frame_callback;
    FibParseErrorCallback error_callback;
    void* callback_context;
} FibParser;

void fib_parser_init(
    FibParser* parser,
    FibFrameCallback frame_callback,
    FibParseErrorCallback error_callback,
    void* context);
void fib_parser_reset(FibParser* parser);
void fib_parser_feed(FibParser* parser, const uint8_t* data, size_t length);

uint32_t fib_crc32(const uint8_t* data, size_t length);

size_t fib_frame_encoded_size(uint32_t payload_length);
bool fib_frame_encode(const FibFrame* frame, uint8_t* output, size_t capacity, size_t* written);

uint16_t fib_read_u16_le(const uint8_t* bytes);
uint32_t fib_read_u32_le(const uint8_t* bytes);
uint64_t fib_read_u64_le(const uint8_t* bytes);
void fib_write_u16_le(uint8_t* bytes, uint16_t value);
void fib_write_u32_le(uint8_t* bytes, uint32_t value);
void fib_write_u64_le(uint8_t* bytes, uint64_t value);

#ifdef __cplusplus
}
#endif
