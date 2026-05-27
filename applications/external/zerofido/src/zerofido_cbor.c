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

#define ZF_CBOR_IMPLEMENTATION
#include "zerofido_cbor.h"

#include <string.h>

#ifdef zf_cbor_encoder_init
#undef zf_cbor_encoder_init
#endif
#ifdef zf_cbor_encoder_size
#undef zf_cbor_encoder_size
#endif
#ifdef zf_cbor_cursor_init
#undef zf_cbor_cursor_init
#endif

enum {
    ZF_CBOR_MAJOR_UINT = 0,
    ZF_CBOR_MAJOR_NEGINT = 1,
    ZF_CBOR_MAJOR_BYTES = 2,
    ZF_CBOR_MAJOR_TEXT = 3,
    ZF_CBOR_MAJOR_ARRAY = 4,
    ZF_CBOR_MAJOR_MAP = 5,
    ZF_CBOR_MAJOR_SIMPLE = 7,
};

#define ZF_CBOR_SKIP_DEPTH_LIMIT 16U

/*
 * CTAP uses strict definite-length CBOR here: no indefinite strings/maps, no
 * non-minimal integer encodings, UTF-8 text is validated, and recursive skip is
 * depth-limited. Parsers rely on those properties when skipping unknown fields.
 */

static size_t zf_cbor_remaining(const ZfCborCursor *cursor) {
    return (size_t)(cursor->end - cursor->ptr);
}

static bool zf_cbor_utf8_is_valid(const uint8_t *ptr, size_t size) {
    for (size_t i = 0; i < size;) {
        uint8_t lead = ptr[i];

        if (lead <= 0x7F) {
            ++i;
            continue;
        }
        if (lead >= 0xC2 && lead <= 0xDF) {
            if (i + 1 >= size || (ptr[i + 1] & 0xC0U) != 0x80U) {
                return false;
            }
            i += 2;
            continue;
        }
        if (lead == 0xE0) {
            if (i + 2 >= size || ptr[i + 1] < 0xA0 || ptr[i + 1] > 0xBF ||
                (ptr[i + 2] & 0xC0U) != 0x80U) {
                return false;
            }
            i += 3;
            continue;
        }
        if ((lead >= 0xE1 && lead <= 0xEC) || (lead >= 0xEE && lead <= 0xEF)) {
            if (i + 2 >= size || (ptr[i + 1] & 0xC0U) != 0x80U || (ptr[i + 2] & 0xC0U) != 0x80U) {
                return false;
            }
            i += 3;
            continue;
        }
        if (lead == 0xED) {
            if (i + 2 >= size || ptr[i + 1] < 0x80 || ptr[i + 1] > 0x9F ||
                (ptr[i + 2] & 0xC0U) != 0x80U) {
                return false;
            }
            i += 3;
            continue;
        }
        if (lead == 0xF0) {
            if (i + 3 >= size || ptr[i + 1] < 0x90 || ptr[i + 1] > 0xBF ||
                (ptr[i + 2] & 0xC0U) != 0x80U || (ptr[i + 3] & 0xC0U) != 0x80U) {
                return false;
            }
            i += 4;
            continue;
        }
        if (lead >= 0xF1 && lead <= 0xF3) {
            if (i + 3 >= size || (ptr[i + 1] & 0xC0U) != 0x80U || (ptr[i + 2] & 0xC0U) != 0x80U ||
                (ptr[i + 3] & 0xC0U) != 0x80U) {
                return false;
            }
            i += 4;
            continue;
        }
        if (lead == 0xF4) {
            if (i + 3 >= size || ptr[i + 1] < 0x80 || ptr[i + 1] > 0x8F ||
                (ptr[i + 2] & 0xC0U) != 0x80U || (ptr[i + 3] & 0xC0U) != 0x80U) {
                return false;
            }
            i += 4;
            continue;
        }

        return false;
    }

    return true;
}

static bool zf_cbor_append(ZfCborEncoder *enc, const void *data, size_t size) {
    if (!enc || enc->offset > enc->capacity || size > enc->capacity - enc->offset ||
        (size > 0 && !data)) {
        return false;
    }
    memcpy(enc->buf + enc->offset, data, size);
    enc->offset += size;
    return true;
}

static bool zf_cbor_encode_head(ZfCborEncoder *enc, uint8_t major, uint64_t value) {
    uint8_t head[9];
    size_t head_len = 1;

    if (value < 24) {
        head[0] = (uint8_t)((major << 5) | value);
    } else if (value <= UINT8_MAX) {
        head[0] = (uint8_t)((major << 5) | 24);
        head[1] = (uint8_t)value;
        head_len = 2;
    } else if (value <= UINT16_MAX) {
        head[0] = (uint8_t)((major << 5) | 25);
        head[1] = (uint8_t)(value >> 8);
        head[2] = (uint8_t)value;
        head_len = 3;
    } else if (value <= UINT32_MAX) {
        head[0] = (uint8_t)((major << 5) | 26);
        head[1] = (uint8_t)(value >> 24);
        head[2] = (uint8_t)(value >> 16);
        head[3] = (uint8_t)(value >> 8);
        head[4] = (uint8_t)value;
        head_len = 5;
    } else {
        head[0] = (uint8_t)((major << 5) | 27);
        for (size_t i = 0; i < 8; ++i) {
            head[i + 1] = (uint8_t)(value >> ((7 - i) * 8));
        }
        head_len = 9;
    }

    return zf_cbor_append(enc, head, head_len);
}

bool zf_cbor_encoder_init(ZfCborEncoder *enc, uint8_t *buf, size_t capacity) {
    if (!enc || !buf || capacity == 0) {
        return false;
    }

    enc->buf = buf;
    enc->capacity = capacity;
    enc->offset = 0;
    return true;
}

size_t zf_cbor_encoder_size(const ZfCborEncoder *enc) {
    return enc->offset;
}

bool zf_cbor_encode_uint(ZfCborEncoder *enc, uint64_t value) {
    return zf_cbor_encode_head(enc, ZF_CBOR_MAJOR_UINT, value);
}

bool zf_cbor_encode_int(ZfCborEncoder *enc, int64_t value) {
    if (value >= 0) {
        return zf_cbor_encode_head(enc, ZF_CBOR_MAJOR_UINT, (uint64_t)value);
    }

    return zf_cbor_encode_head(enc, ZF_CBOR_MAJOR_NEGINT, (uint64_t)(-1 - value));
}

bool zf_cbor_encode_bool(ZfCborEncoder *enc, bool value) {
    uint8_t byte = (ZF_CBOR_MAJOR_SIMPLE << 5) | (uint8_t)(value ? 21 : 20);
    return zf_cbor_append(enc, &byte, 1);
}

bool zf_cbor_encode_bytes(ZfCborEncoder *enc, const uint8_t *data, size_t size) {
    return zf_cbor_encode_head(enc, ZF_CBOR_MAJOR_BYTES, size) &&
           (size == 0 || zf_cbor_append(enc, data, size));
}

bool zf_cbor_reserve_bytes(ZfCborEncoder *enc, size_t size, uint8_t **out) {
    size_t old_offset = 0;

    if (!enc || !out) {
        return false;
    }
    old_offset = enc->offset;
    if (!zf_cbor_encode_head(enc, ZF_CBOR_MAJOR_BYTES, size)) {
        return false;
    }
    if (enc->offset > enc->capacity || size > enc->capacity - enc->offset) {
        enc->offset = old_offset;
        return false;
    }
    *out = enc->buf + enc->offset;
    enc->offset += size;
    return true;
}

bool zf_cbor_encode_text(ZfCborEncoder *enc, const char *text) {
    return zf_cbor_encode_text_n(enc, text, strlen(text));
}

bool zf_cbor_encode_text_n(ZfCborEncoder *enc, const char *text, size_t size) {
    return zf_cbor_encode_head(enc, ZF_CBOR_MAJOR_TEXT, size) &&
           (size == 0 || zf_cbor_append(enc, text, size));
}

bool zf_cbor_encode_map(ZfCborEncoder *enc, size_t pairs) {
    return zf_cbor_encode_head(enc, ZF_CBOR_MAJOR_MAP, pairs);
}

bool zf_cbor_encode_array(ZfCborEncoder *enc, size_t items) {
    return zf_cbor_encode_head(enc, ZF_CBOR_MAJOR_ARRAY, items);
}

bool zf_cbor_encode_p256_cose_key(ZfCborEncoder *enc, int64_t alg, const uint8_t *x, size_t x_size,
                                  const uint8_t *y, size_t y_size) {
    return zf_cbor_encode_map(enc, 5) && zf_cbor_encode_int(enc, 1) && zf_cbor_encode_int(enc, 2) &&
           zf_cbor_encode_int(enc, 3) && zf_cbor_encode_int(enc, alg) &&
           zf_cbor_encode_int(enc, -1) && zf_cbor_encode_int(enc, 1) &&
           zf_cbor_encode_int(enc, -2) && zf_cbor_encode_bytes(enc, x, x_size) &&
           zf_cbor_encode_int(enc, -3) && zf_cbor_encode_bytes(enc, y, y_size);
}

void zf_cbor_cursor_init(ZfCborCursor *cursor, const uint8_t *data, size_t size) {
    cursor->ptr = data;
    cursor->end = data + size;
}

static bool zf_cbor_read_head(ZfCborCursor *cursor, uint8_t *major, uint64_t *value) {
    if (cursor->ptr >= cursor->end) {
        return false;
    }

    uint8_t byte = *cursor->ptr++;
    *major = byte >> 5;
    uint8_t info = byte & 0x1F;

    if (info < 24) {
        *value = info;
        return true;
    }
    if (info == 24) {
        if (zf_cbor_remaining(cursor) < 1) {
            return false;
        }
        *value = *cursor->ptr++;
        return *value >= 24;
    }
    if (info == 25) {
        if (zf_cbor_remaining(cursor) < 2) {
            return false;
        }
        *value = ((uint64_t)cursor->ptr[0] << 8) | cursor->ptr[1];
        cursor->ptr += 2;
        return *value > UINT8_MAX;
    }
    if (info == 26) {
        if (zf_cbor_remaining(cursor) < 4) {
            return false;
        }
        *value = ((uint64_t)cursor->ptr[0] << 24) | ((uint64_t)cursor->ptr[1] << 16) |
                 ((uint64_t)cursor->ptr[2] << 8) | cursor->ptr[3];
        cursor->ptr += 4;
        return *value > UINT16_MAX;
    }
    if (info == 27) {
        if (zf_cbor_remaining(cursor) < 8) {
            return false;
        }
        *value = ((uint64_t)cursor->ptr[0] << 56) | ((uint64_t)cursor->ptr[1] << 48) |
                 ((uint64_t)cursor->ptr[2] << 40) | ((uint64_t)cursor->ptr[3] << 32) |
                 ((uint64_t)cursor->ptr[4] << 24) | ((uint64_t)cursor->ptr[5] << 16) |
                 ((uint64_t)cursor->ptr[6] << 8) | cursor->ptr[7];
        cursor->ptr += 8;
        return *value > UINT32_MAX;
    }

    return false;
}

bool zf_cbor_read_uint(ZfCborCursor *cursor, uint64_t *value) {
    uint8_t major = 0;
    if (!zf_cbor_read_head(cursor, &major, value)) {
        return false;
    }
    return major == ZF_CBOR_MAJOR_UINT;
}

bool zf_cbor_read_int(ZfCborCursor *cursor, int64_t *value) {
    uint8_t major = 0;
    uint64_t raw = 0;

    if (!zf_cbor_read_head(cursor, &major, &raw)) {
        return false;
    }
    if (major == ZF_CBOR_MAJOR_UINT) {
        if (raw > INT64_MAX) {
            return false;
        }
        *value = (int64_t)raw;
        return true;
    }
    if (major == ZF_CBOR_MAJOR_NEGINT) {
        if (raw > INT64_MAX) {
            return false;
        }
        *value = -1 - (int64_t)raw;
        return true;
    }
    return false;
}

bool zf_cbor_read_bool(ZfCborCursor *cursor, bool *value) {
    if (cursor->ptr >= cursor->end) {
        return false;
    }

    uint8_t byte = *cursor->ptr++;
    if (byte == ((ZF_CBOR_MAJOR_SIMPLE << 5) | 20)) {
        *value = false;
        return true;
    }
    if (byte == ((ZF_CBOR_MAJOR_SIMPLE << 5) | 21)) {
        *value = true;
        return true;
    }
    return false;
}

bool zf_cbor_read_bytes_ptr(ZfCborCursor *cursor, const uint8_t **data, size_t *size) {
    uint8_t major = 0;
    uint64_t raw = 0;
    size_t remaining = 0;

    if (!zf_cbor_read_head(cursor, &major, &raw) || major != ZF_CBOR_MAJOR_BYTES) {
        return false;
    }
    remaining = zf_cbor_remaining(cursor);
    if (raw > (uint64_t)remaining) {
        return false;
    }

    *data = cursor->ptr;
    *size = (size_t)raw;
    cursor->ptr += raw;
    return true;
}

bool zf_cbor_read_text_ptr(ZfCborCursor *cursor, const uint8_t **data, size_t *size) {
    uint8_t major = 0;
    uint64_t raw = 0;
    size_t remaining = 0;

    if (!zf_cbor_read_head(cursor, &major, &raw) || major != ZF_CBOR_MAJOR_TEXT) {
        return false;
    }
    remaining = zf_cbor_remaining(cursor);
    if (raw > (uint64_t)remaining) {
        return false;
    }

    *data = cursor->ptr;
    *size = (size_t)raw;
    if (!zf_cbor_utf8_is_valid(*data, *size)) {
        return false;
    }
    cursor->ptr += raw;
    return true;
}

bool zf_cbor_read_map_start(ZfCborCursor *cursor, size_t *pairs) {
    uint8_t major = 0;
    uint64_t raw = 0;

    if (!zf_cbor_read_head(cursor, &major, &raw) || major != ZF_CBOR_MAJOR_MAP) {
        return false;
    }
    if (raw > (uint64_t)SIZE_MAX) {
        return false;
    }

    *pairs = (size_t)raw;
    return true;
}

bool zf_cbor_read_array_start(ZfCborCursor *cursor, size_t *items) {
    uint8_t major = 0;
    uint64_t raw = 0;

    if (!zf_cbor_read_head(cursor, &major, &raw) || major != ZF_CBOR_MAJOR_ARRAY) {
        return false;
    }
    if (raw > (uint64_t)SIZE_MAX) {
        return false;
    }

    *items = (size_t)raw;
    return true;
}

static bool zf_cbor_skip_read_head(ZfCborCursor *cursor, uint8_t *major, uint64_t *value) {
    if (cursor->ptr >= cursor->end) {
        return false;
    }

    uint8_t byte = *cursor->ptr++;
    uint8_t info = byte & 0x1F;
    *major = byte >> 5;

    if (info < 24) {
        *value = info;
        return true;
    }
    if (info == 24) {
        if (zf_cbor_remaining(cursor) < 1) {
            return false;
        }
        *value = *cursor->ptr++;
        return true;
    }
    if (info == 25) {
        if (zf_cbor_remaining(cursor) < 2) {
            return false;
        }
        *value = ((uint64_t)cursor->ptr[0] << 8) | cursor->ptr[1];
        cursor->ptr += 2;
        return true;
    }
    if (info == 26) {
        if (zf_cbor_remaining(cursor) < 4) {
            return false;
        }
        *value = ((uint64_t)cursor->ptr[0] << 24) | ((uint64_t)cursor->ptr[1] << 16) |
                 ((uint64_t)cursor->ptr[2] << 8) | cursor->ptr[3];
        cursor->ptr += 4;
        return true;
    }
    if (info == 27) {
        if (zf_cbor_remaining(cursor) < 8) {
            return false;
        }
        *value = ((uint64_t)cursor->ptr[0] << 56) | ((uint64_t)cursor->ptr[1] << 48) |
                 ((uint64_t)cursor->ptr[2] << 40) | ((uint64_t)cursor->ptr[3] << 32) |
                 ((uint64_t)cursor->ptr[4] << 24) | ((uint64_t)cursor->ptr[5] << 16) |
                 ((uint64_t)cursor->ptr[6] << 8) | cursor->ptr[7];
        cursor->ptr += 8;
        return true;
    }

    return false;
}

static bool zf_cbor_skip_inner(ZfCborCursor *cursor, size_t depth) {
    uint8_t major = 0;
    uint64_t raw = 0;

    if (depth > ZF_CBOR_SKIP_DEPTH_LIMIT) {
        return false;
    }
    if (!zf_cbor_skip_read_head(cursor, &major, &raw)) {
        return false;
    }

    switch (major) {
    case ZF_CBOR_MAJOR_UINT:
    case ZF_CBOR_MAJOR_NEGINT:
        return true;
    case ZF_CBOR_MAJOR_SIMPLE:
        return raw != 31;
    case ZF_CBOR_MAJOR_BYTES:
    case ZF_CBOR_MAJOR_TEXT:
        if (raw > (uint64_t)zf_cbor_remaining(cursor)) {
            return false;
        }
        cursor->ptr += (size_t)raw;
        return true;
    case ZF_CBOR_MAJOR_ARRAY:
        for (size_t i = 0; i < raw; ++i) {
            if (!zf_cbor_skip_inner(cursor, depth + 1)) {
                return false;
            }
        }
        return true;
    case ZF_CBOR_MAJOR_MAP:
        for (size_t i = 0; i < raw; ++i) {
            if (!zf_cbor_skip_inner(cursor, depth + 1) || !zf_cbor_skip_inner(cursor, depth + 1)) {
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}

bool zf_cbor_skip(ZfCborCursor *cursor) {
    return zf_cbor_skip_inner(cursor, 0);
}
