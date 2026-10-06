#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// CBOR encoder context
typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} CborEncoder;

// Initialize encoder with buffer
void cbor_encoder_init(CborEncoder* encoder, uint8_t* buffer, size_t capacity);

// Encode unsigned integer
bool cbor_encode_uint(CborEncoder* encoder, uint64_t value);

// Encode signed integer (negative)
bool cbor_encode_int(CborEncoder* encoder, int64_t value);

// Encode byte string
bool cbor_encode_bytes(CborEncoder* encoder, const uint8_t* data, size_t length);

// Encode text string
bool cbor_encode_text(CborEncoder* encoder, const char* text, size_t length);

// Encode map header (definite length)
bool cbor_encode_map(CborEncoder* encoder, size_t pair_count);

// Get current encoded size
size_t cbor_encoder_get_size(const CborEncoder* encoder);

// Get remaining space
size_t cbor_encoder_remaining(const CborEncoder* encoder);
