#pragma once

#include <stdint.h>
#include <stddef.h>

/** Decode one UTF-8 code point. @returns bytes consumed, 0 on invalid input. */
size_t utf8_decode(const char* text, size_t size, uint32_t* code_point);

/** Encode a code point into out (at least 4 bytes). @returns bytes written, 0 on error. */
size_t utf8_encode(uint32_t code_point, char* out);
