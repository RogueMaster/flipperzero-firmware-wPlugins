#pragma once

#include <stddef.h>
#include <stdint.h>

/* Decodes a hex string into out, returning the byte count. */
size_t hex_to_bytes(const char* hex, uint8_t* out, size_t out_cap);
