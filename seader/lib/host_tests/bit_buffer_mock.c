#include "bit_buffer.h"

#include <stdlib.h>
#include <string.h>

struct BitBuffer {
    uint8_t* data;
    size_t size_bits;
    size_t capacity_bytes;
};

BitBuffer* bit_buffer_alloc(size_t capacity_bytes) {
    BitBuffer* buf = malloc(sizeof(BitBuffer));
    buf->data = calloc(1, capacity_bytes);
    buf->size_bits = 0;
    buf->capacity_bytes = capacity_bytes;
    return buf;
}

void bit_buffer_free(BitBuffer* buf) {
    if(buf) {
        free(buf->data);
        free(buf);
    }
}

void bit_buffer_reset(BitBuffer* buf) {
    buf->size_bits = 0;
}

void bit_buffer_copy_bytes(BitBuffer* buf, const uint8_t* data, size_t size_bytes) {
    memcpy(buf->data, data, size_bytes);
    buf->size_bits = size_bytes * 8;
}

size_t bit_buffer_get_size_bytes(const BitBuffer* buf) {
    return (buf->size_bits + 7) / 8;
}

const uint8_t* bit_buffer_get_data(const BitBuffer* buf) {
    return buf->data;
}

void bit_buffer_append_bytes(BitBuffer* buf, const uint8_t* data, size_t size_bytes) {
    size_t current_bytes = bit_buffer_get_size_bytes(buf);
    memcpy(buf->data + current_bytes, data, size_bytes);
    buf->size_bits += size_bytes * 8;
}
