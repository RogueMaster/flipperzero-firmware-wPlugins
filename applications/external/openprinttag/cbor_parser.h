#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// CBOR major types
#define CBOR_TYPE_UINT  0
#define CBOR_TYPE_NINT  1
#define CBOR_TYPE_BYTES 2
#define CBOR_TYPE_TEXT  3
#define CBOR_TYPE_ARRAY 4
#define CBOR_TYPE_MAP   5
#define CBOR_TYPE_TAG   6
#define CBOR_TYPE_FLOAT 7

// CBOR additional info values
#define CBOR_INFO_UINT8  24
#define CBOR_INFO_UINT16 25
#define CBOR_INFO_UINT32 26
#define CBOR_INFO_UINT64 27

typedef struct {
    const uint8_t* data;
    size_t size;
    size_t offset;
} CborParser;

typedef enum {
    CborValueTypeUnsigned,
    CborValueTypeSigned,
    CborValueTypeBytes,
    CborValueTypeText,
    CborValueTypeMap,
    CborValueTypeArray,
    CborValueTypeUnknown,
} CborValueType;

typedef struct {
    CborValueType type;
    union {
        uint64_t u64;
        int64_t i64;
        struct {
            const uint8_t* data;
            size_t size;
        } bytes;
        struct {
            const uint8_t* data;
            size_t size;
        } text;
        size_t count; // For maps and arrays
    } value;
} CborValue;

// Initialize parser
void cbor_parser_init(CborParser* parser, const uint8_t* data, size_t size);

// Parse CBOR value
bool cbor_parse_value(CborParser* parser, CborValue* value);

// Parse CBOR map - returns number of key-value pairs
bool cbor_parse_map(CborParser* parser, size_t* count);

// Helper to skip a value
bool cbor_skip_value(CborParser* parser);

// Get remaining bytes
size_t cbor_parser_remaining(const CborParser* parser);
