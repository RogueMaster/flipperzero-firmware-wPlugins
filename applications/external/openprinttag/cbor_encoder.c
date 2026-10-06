#include "cbor_encoder.h"
#include <string.h>

// CBOR major types
#define CBOR_MAJOR_UINT  0
#define CBOR_MAJOR_NINT  1
#define CBOR_MAJOR_BYTES 2
#define CBOR_MAJOR_TEXT  3
#define CBOR_MAJOR_MAP   5

// CBOR additional info values
#define CBOR_INFO_UINT8  24
#define CBOR_INFO_UINT16 25
#define CBOR_INFO_UINT32 26
#define CBOR_INFO_UINT64 27

void cbor_encoder_init(CborEncoder* encoder, uint8_t* buffer, size_t capacity) {
    encoder->buffer = buffer;
    encoder->capacity = capacity;
    encoder->offset = 0;
}

static bool cbor_write_byte(CborEncoder* encoder, uint8_t byte) {
    if(encoder->offset >= encoder->capacity) {
        return false;
    }
    encoder->buffer[encoder->offset++] = byte;
    return true;
}

static bool cbor_write_bytes(CborEncoder* encoder, const uint8_t* data, size_t length) {
    if(encoder->offset + length > encoder->capacity) {
        return false;
    }
    memcpy(encoder->buffer + encoder->offset, data, length);
    encoder->offset += length;
    return true;
}

static bool cbor_encode_initial_byte(CborEncoder* encoder, uint8_t major, uint64_t value) {
    if(value < 24) {
        return cbor_write_byte(encoder, (major << 5) | (uint8_t)value);
    } else if(value <= 0xFF) {
        if(!cbor_write_byte(encoder, (major << 5) | CBOR_INFO_UINT8)) return false;
        return cbor_write_byte(encoder, (uint8_t)value);
    } else if(value <= 0xFFFF) {
        if(!cbor_write_byte(encoder, (major << 5) | CBOR_INFO_UINT16)) return false;
        uint8_t bytes[2] = {(uint8_t)(value >> 8), (uint8_t)value};
        return cbor_write_bytes(encoder, bytes, 2);
    } else if(value <= 0xFFFFFFFF) {
        if(!cbor_write_byte(encoder, (major << 5) | CBOR_INFO_UINT32)) return false;
        uint8_t bytes[4] = {
            (uint8_t)(value >> 24), (uint8_t)(value >> 16), (uint8_t)(value >> 8), (uint8_t)value};
        return cbor_write_bytes(encoder, bytes, 4);
    } else {
        if(!cbor_write_byte(encoder, (major << 5) | CBOR_INFO_UINT64)) return false;
        uint8_t bytes[8] = {
            (uint8_t)(value >> 56),
            (uint8_t)(value >> 48),
            (uint8_t)(value >> 40),
            (uint8_t)(value >> 32),
            (uint8_t)(value >> 24),
            (uint8_t)(value >> 16),
            (uint8_t)(value >> 8),
            (uint8_t)value};
        return cbor_write_bytes(encoder, bytes, 8);
    }
}

bool cbor_encode_uint(CborEncoder* encoder, uint64_t value) {
    return cbor_encode_initial_byte(encoder, CBOR_MAJOR_UINT, value);
}

bool cbor_encode_int(CborEncoder* encoder, int64_t value) {
    if(value >= 0) {
        return cbor_encode_uint(encoder, (uint64_t)value);
    } else {
        // For negative integers, encode -1 - value
        uint64_t encoded = (uint64_t)(-1 - value);
        return cbor_encode_initial_byte(encoder, CBOR_MAJOR_NINT, encoded);
    }
}

bool cbor_encode_bytes(CborEncoder* encoder, const uint8_t* data, size_t length) {
    if(!cbor_encode_initial_byte(encoder, CBOR_MAJOR_BYTES, length)) {
        return false;
    }
    return cbor_write_bytes(encoder, data, length);
}

bool cbor_encode_text(CborEncoder* encoder, const char* text, size_t length) {
    if(!cbor_encode_initial_byte(encoder, CBOR_MAJOR_TEXT, length)) {
        return false;
    }
    return cbor_write_bytes(encoder, (const uint8_t*)text, length);
}

bool cbor_encode_map(CborEncoder* encoder, size_t pair_count) {
    return cbor_encode_initial_byte(encoder, CBOR_MAJOR_MAP, pair_count);
}

size_t cbor_encoder_get_size(const CborEncoder* encoder) {
    return encoder->offset;
}

size_t cbor_encoder_remaining(const CborEncoder* encoder) {
    return encoder->capacity - encoder->offset;
}
