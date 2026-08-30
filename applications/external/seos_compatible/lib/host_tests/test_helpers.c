#include "test_helpers.h"

#include <furi.h>

static int nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t hex_to_bytes(const char* hex, uint8_t* out, size_t out_cap) {
    size_t len = strlen(hex);
    furi_check(len % 2 == 0);
    furi_check(len / 2 <= out_cap);

    for(size_t i = 0; i < len; i += 2) {
        int hi = nibble(hex[i]);
        int lo = nibble(hex[i + 1]);
        furi_check(hi >= 0 && lo >= 0);
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return len / 2;
}
