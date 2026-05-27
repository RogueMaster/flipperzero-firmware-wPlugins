/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;
    size_t capacity;
    size_t offset;
} ZfCborEncoder;

typedef struct {
    const uint8_t *ptr;
    const uint8_t *end;
} ZfCborCursor;

/*
 * Minimal definite-length CBOR encoder/decoder for CTAP payloads. It rejects
 * malformed input by returning false and never allocates; callers enforce each
 * command's map keys and semantic constraints.
 */
#if defined(ZF_CBOR_IMPLEMENTATION)
bool zf_cbor_encoder_init(ZfCborEncoder *enc, uint8_t *buf, size_t capacity);
#else
static inline bool zf_cbor_encoder_init_inline(ZfCborEncoder *enc, uint8_t *buf, size_t capacity) {
    if (!enc || !buf || capacity == 0) {
        return false;
    }

    enc->buf = buf;
    enc->capacity = capacity;
    enc->offset = 0;
    return true;
}
#define zf_cbor_encoder_init(enc, buf, capacity) zf_cbor_encoder_init_inline((enc), (buf), (capacity))
#endif
#if defined(ZF_CBOR_IMPLEMENTATION)
size_t zf_cbor_encoder_size(const ZfCborEncoder *enc);
#else
static inline size_t zf_cbor_encoder_size_inline(const ZfCborEncoder *enc) {
    return enc->offset;
}
#define zf_cbor_encoder_size(enc) zf_cbor_encoder_size_inline(enc)
#endif
bool zf_cbor_encode_uint(ZfCborEncoder *enc, uint64_t value);
bool zf_cbor_encode_int(ZfCborEncoder *enc, int64_t value);
bool zf_cbor_encode_bool(ZfCborEncoder *enc, bool value);
bool zf_cbor_encode_bytes(ZfCborEncoder *enc, const uint8_t *data, size_t size);
bool zf_cbor_reserve_bytes(ZfCborEncoder *enc, size_t size, uint8_t **out);
bool zf_cbor_encode_text(ZfCborEncoder *enc, const char *text);
bool zf_cbor_encode_text_n(ZfCborEncoder *enc, const char *text, size_t size);
bool zf_cbor_encode_map(ZfCborEncoder *enc, size_t pairs);
bool zf_cbor_encode_array(ZfCborEncoder *enc, size_t items);
bool zf_cbor_encode_p256_cose_key(ZfCborEncoder *enc, int64_t alg, const uint8_t *x, size_t x_size,
                                  const uint8_t *y, size_t y_size);

#if defined(ZF_CBOR_IMPLEMENTATION)
void zf_cbor_cursor_init(ZfCborCursor *cursor, const uint8_t *data, size_t size);
#else
static inline void zf_cbor_cursor_init_inline(ZfCborCursor *cursor, const uint8_t *data,
                                              size_t size) {
    cursor->ptr = data;
    cursor->end = data + size;
}
#define zf_cbor_cursor_init(cursor, data, size) zf_cbor_cursor_init_inline((cursor), (data), (size))
#endif
bool zf_cbor_read_uint(ZfCborCursor *cursor, uint64_t *value);
bool zf_cbor_read_int(ZfCborCursor *cursor, int64_t *value);
bool zf_cbor_read_bool(ZfCborCursor *cursor, bool *value);
bool zf_cbor_read_bytes_ptr(ZfCborCursor *cursor, const uint8_t **data, size_t *size);
bool zf_cbor_read_text_ptr(ZfCborCursor *cursor, const uint8_t **data, size_t *size);
bool zf_cbor_read_map_start(ZfCborCursor *cursor, size_t *pairs);
bool zf_cbor_read_array_start(ZfCborCursor *cursor, size_t *items);
bool zf_cbor_skip(ZfCborCursor *cursor);
