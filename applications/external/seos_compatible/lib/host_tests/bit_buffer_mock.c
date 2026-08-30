#include <lib/toolbox/bit_buffer.h>
#include <lib/toolbox/hex.h>

#include <furi.h>

struct BitBuffer {
    uint8_t* data;
    size_t size_bytes;
    size_t capacity_bytes;
};

BitBuffer* bit_buffer_alloc(size_t capacity_bytes) {
    furi_check(capacity_bytes);
    BitBuffer* buf = malloc(sizeof(BitBuffer));
    furi_check(buf);
    buf->data = calloc(1, capacity_bytes);
    furi_check(buf->data);
    buf->size_bytes = 0;
    buf->capacity_bytes = capacity_bytes;
    return buf;
}

void bit_buffer_free(BitBuffer* buf) {
    if(!buf) return;
    free(buf->data);
    free(buf);
}

void bit_buffer_reset(BitBuffer* buf) {
    furi_check(buf);
    buf->size_bytes = 0;
    memset(buf->data, 0, buf->capacity_bytes);
}

void bit_buffer_copy_bytes(BitBuffer* buf, const uint8_t* data, size_t size_bytes) {
    furi_check(buf);
    furi_check(data);
    furi_check(buf->capacity_bytes >= size_bytes);
    memcpy(buf->data, data, size_bytes);
    buf->size_bytes = size_bytes;
}

void bit_buffer_append_bytes(BitBuffer* buf, const uint8_t* data, size_t size_bytes) {
    furi_check(buf);
    furi_check(data);
    furi_check(buf->capacity_bytes >= buf->size_bytes + size_bytes);
    memcpy(buf->data + buf->size_bytes, data, size_bytes);
    buf->size_bytes += size_bytes;
}

void bit_buffer_append_byte(BitBuffer* buf, uint8_t byte) {
    bit_buffer_append_bytes(buf, &byte, 1);
}

size_t bit_buffer_get_size_bytes(const BitBuffer* buf) {
    furi_check(buf);
    return buf->size_bytes;
}

size_t bit_buffer_get_capacity_bytes(const BitBuffer* buf) {
    furi_check(buf);
    return buf->capacity_bytes;
}

const uint8_t* bit_buffer_get_data(const BitBuffer* buf) {
    furi_check(buf);
    return buf->data;
}

uint8_t bit_buffer_get_byte(const BitBuffer* buf, size_t index) {
    furi_check(buf);
    furi_check(index < buf->size_bytes);
    return buf->data[index];
}

void uint8_to_hex_chars(const uint8_t* src, uint8_t* target, int length) {
    const char chars[] = "0123456789ABCDEF";
    while(--length >= 0)
        target[length] = chars[(src[length >> 1] >> ((1 - (length & 1)) << 2)) & 0xF];
}
