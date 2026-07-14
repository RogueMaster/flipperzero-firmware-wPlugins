#include "hex_converter.h"

uint8_t hex2uint8_nibble(char* nibble_ptr) {
    char nibble = *nibble_ptr;
    if(nibble >= '0' && nibble <= '9') return nibble - '0';
    if(nibble >= 'A' && nibble <= 'F') return nibble - 'A' + 10;
    return 0;
}

uint8_t hex2uint8(char* nibble_high, char* nibble_low) {
    return (uint8_t)((hex2uint8_nibble(nibble_high) << 4) + hex2uint8_nibble(nibble_low));
}
