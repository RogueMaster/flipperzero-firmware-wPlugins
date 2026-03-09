#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BitBuffer BitBuffer;

BitBuffer* bit_buffer_alloc(size_t capacity_bytes);
void bit_buffer_free(BitBuffer* buf);
void bit_buffer_reset(BitBuffer* buf);
void bit_buffer_copy_bytes(BitBuffer* buf, const uint8_t* data, size_t size_bytes);
size_t bit_buffer_get_size_bytes(const BitBuffer* buf);
const uint8_t* bit_buffer_get_data(const BitBuffer* buf);
void bit_buffer_append_bytes(BitBuffer* buf, const uint8_t* data, size_t size_bytes);
