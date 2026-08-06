#include "functions.h"

typedef enum {
    UINT2BYTES,
    BYTES2UINT,
} CONVERT_TYPE;

void converter(void* value, uint8_t* bytes, uint8_t bytes_size, CONVERT_TYPE convert_type) {
    for(uint8_t i = 0; i < bytes_size; i++) {
        if(convert_type == UINT2BYTES)
            bytes[i] = ((uint8_t*)value)[bytes_size - 1 - i];
        else if(convert_type == BYTES2UINT)
            ((uint8_t*)value)[bytes_size - 1 - i] = bytes[i];
    }
}

// Helper function to convert n*8-bit value to network byte order (big-endian)
void uint_to_bytes(void* value, uint8_t* bytes, uint8_t bytes_size) {
    converter(value, bytes, bytes_size, UINT2BYTES);
}

void bytes_to_uint(void* value, uint8_t* bytes, uint8_t bytes_size) {
    converter(value, bytes, bytes_size, BYTES2UINT);
}
