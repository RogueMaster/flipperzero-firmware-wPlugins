/**
 * @file utf8.c
 * @brief Strict UTF-8 decoder and encoder.
 */

#include "utf8.h"

#include <stdbool.h>

/** Largest legal code point, U+10FFFF. */
#define UTF8_MAX_CODE_POINT 0x10FFFFu

/** First and last UTF-16 surrogate code points, never valid in UTF-8. */
#define UTF8_SURROGATE_FIRST 0xD800u
#define UTF8_SURROGATE_LAST  0xDFFFu

static bool utf8_is_continuation(unsigned char byte) {
    return (byte & 0xC0u) == 0x80u;
}

static bool utf8_is_surrogate(uint32_t code_point) {
    return code_point >= UTF8_SURROGATE_FIRST && code_point <= UTF8_SURROGATE_LAST;
}

size_t utf8_decode(const char* text, size_t size, uint32_t* code_point) {
    if(text == NULL || size == 0) {
        return 0;
    }

    const unsigned char* bytes = (const unsigned char*)text;
    unsigned char lead = bytes[0];

    if(lead < 0x80u) {
        if(code_point != NULL) {
            *code_point = lead;
        }
        return 1;
    }

    size_t continuation;
    uint32_t value;
    uint32_t minimum;

    if((lead & 0xE0u) == 0xC0u) {
        continuation = 1;
        value = lead & 0x1Fu;
        minimum = 0x80u;
    } else if((lead & 0xF0u) == 0xE0u) {
        continuation = 2;
        value = lead & 0x0Fu;
        minimum = 0x800u;
    } else if((lead & 0xF8u) == 0xF0u) {
        continuation = 3;
        value = lead & 0x07u;
        minimum = 0x10000u;
    } else {
        /* Continuation byte or 5+ byte lead: never valid. */
        return 0;
    }

    if(size < continuation + 1) {
        /* Truncated sequence. */
        return 0;
    }

    for(size_t i = 1; i <= continuation; i++) {
        if(!utf8_is_continuation(bytes[i])) {
            return 0;
        }
        value = (value << 6) | (bytes[i] & 0x3Fu);
    }

    if(value < minimum || value > UTF8_MAX_CODE_POINT || utf8_is_surrogate(value)) {
        return 0;
    }

    if(code_point != NULL) {
        *code_point = value;
    }
    return continuation + 1;
}

size_t utf8_encode(uint32_t code_point, char* out) {
    if(out == NULL || code_point > UTF8_MAX_CODE_POINT || utf8_is_surrogate(code_point)) {
        return 0;
    }

    unsigned char* bytes = (unsigned char*)out;

    if(code_point < 0x80u) {
        bytes[0] = (unsigned char)code_point;
        return 1;
    }
    if(code_point < 0x800u) {
        bytes[0] = (unsigned char)(0xC0u | (code_point >> 6));
        bytes[1] = (unsigned char)(0x80u | (code_point & 0x3Fu));
        return 2;
    }
    if(code_point < 0x10000u) {
        bytes[0] = (unsigned char)(0xE0u | (code_point >> 12));
        bytes[1] = (unsigned char)(0x80u | ((code_point >> 6) & 0x3Fu));
        bytes[2] = (unsigned char)(0x80u | (code_point & 0x3Fu));
        return 3;
    }

    bytes[0] = (unsigned char)(0xF0u | (code_point >> 18));
    bytes[1] = (unsigned char)(0x80u | ((code_point >> 12) & 0x3Fu));
    bytes[2] = (unsigned char)(0x80u | ((code_point >> 6) & 0x3Fu));
    bytes[3] = (unsigned char)(0x80u | (code_point & 0x3Fu));
    return 4;
}
