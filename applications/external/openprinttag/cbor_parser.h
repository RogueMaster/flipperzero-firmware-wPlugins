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
#define CBOR_INFO_UINT8      24
#define CBOR_INFO_UINT16     25
#define CBOR_INFO_UINT32     26
#define CBOR_INFO_UINT64     27
// Indefinite-length maps/arrays, closed by a "break" byte
#define CBOR_INFO_INDEFINITE 31
#define CBOR_BREAK           0xFF

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
    CborValueTypeFloat,
    CborValueTypeSimple, // false (20), true (21), null (22), undefined (23)
    CborValueTypeUnknown,
} CborValueType;

typedef struct {
    CborValueType type;
    bool indefinite; // For maps and arrays: a break byte follows the last element
    union {
        uint64_t u64; // Also holds the simple value for CborValueTypeSimple
        int64_t i64;
        float f32;
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

// Skip the elements of an already parsed map/array value (no-op for scalars).
// Must be called for every container value whose elements the caller does not read.
bool cbor_skip_contents(CborParser* parser, const CborValue* value);

// Get remaining bytes
size_t cbor_parser_remaining(const CborParser* parser);
