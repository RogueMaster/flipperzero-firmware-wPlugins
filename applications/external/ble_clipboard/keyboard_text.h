#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline uint16_t clipboard_turkish_hid(uint32_t scalar) {
    switch(scalar) {
    case 0x00E7:
        return 0x0037;
    case 0x00C7:
        return 0x0237;
    case 0x011F:
        return 0x002F;
    case 0x011E:
        return 0x022F;
    case 0x0131:
        return 0x000C;
    case 0x0130:
        return 0x0234;
    case 0x00F6:
        return 0x0036;
    case 0x00D6:
        return 0x0236;
    case 0x015F:
        return 0x0033;
    case 0x015E:
        return 0x0233;
    case 0x00FC:
        return 0x0030;
    case 0x00DC:
        return 0x0230;
    default:
        return 0;
    }
}

// Only ASCII and the supported two-byte Turkish scalars are accepted on this wire.
static inline bool
    clipboard_text_next(const uint8_t* text, size_t size, size_t* position, uint32_t* scalar) {
    if(*position >= size) return false;
    uint8_t first = text[*position];
    if(first < 128) {
        if((first < 32 || first > 126) && first != '\t' && first != '\n' && first != '\r')
            return false;
        *scalar = first;
        (*position)++;
        return true;
    }
    if((first & 0xE0) != 0xC0 || *position + 1 >= size) return false;
    uint8_t second = text[*position + 1];
    if((second & 0xC0) != 0x80) return false;
    uint32_t value = ((uint32_t)(first & 0x1F) << 6) | (second & 0x3F);
    if(!clipboard_turkish_hid(value)) return false;
    *scalar = value;
    *position += 2;
    return true;
}

static inline bool clipboard_text_valid(const uint8_t* text, size_t size, uint8_t layout) {
    if(!size) return false;
    size_t position = 0;
    while(position < size) {
        uint32_t scalar;
        if(!clipboard_text_next(text, size, &position, &scalar) || (scalar > 127 && layout != 1))
            return false;
    }
    return true;
}
