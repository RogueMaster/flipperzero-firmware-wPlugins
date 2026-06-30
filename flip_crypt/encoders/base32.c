#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base32.h"

char* base32_encode(const uint8_t* data, size_t input_length) {
    const char* base32_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    size_t output_length = (input_length * 8 + 4) / 5;  // ceil(bits / 5)
    size_t encoded_length = (output_length + 7) / 8 * 8; // pad to full 8 chars
    char* encoded = (char*)malloc(encoded_length + 1); // +1 for null terminator

    if (!encoded) return "memory allocation failed, try again";

    size_t buffer = 0;
    int bits_left = 0;
    size_t index = 0;

    for (size_t i = 0; i < input_length; i++) {
        buffer <<= 8;
        buffer |= data[i];
        bits_left += 8;

        while (bits_left >= 5) {
            encoded[index++] = base32_chars[(buffer >> (bits_left - 5)) & 0x1F];
            bits_left -= 5;
        }
    }

    if (bits_left > 0) {
        buffer <<= (5 - bits_left);
        encoded[index++] = base32_chars[buffer & 0x1F];
    }

    while (index < encoded_length) {
        encoded[index++] = '=';
    }

    encoded[index] = '\0';
    return encoded;
}

int base32_char_to_value(char c) {
    if ('A' <= c && c <= 'Z') return c - 'A';
    if ('2' <= c && c <= '7') return c - '2' + 26;
    return -1; // Invalid character
}

uint8_t* base32_decode(const char* input, size_t* output_length) {
    size_t input_length = strlen(input);
    size_t buffer = 0;
    int bits_left = 0;
    size_t index = 0;

    // Allocate maximum possible decoded size
    uint8_t* decoded = (uint8_t*)malloc((input_length * 5 / 8) + 1);
    if (!decoded) return (uint8_t *)"memory allocation failed, try again";

    for (size_t i = 0; i < input_length; i++) {
        char c = toupper(input[i]);

        if (c == '=') break; // Padding, stop reading
        int val = base32_char_to_value(c);
        if (val == -1) continue; // Skip invalid characters

        buffer <<= 5;
        buffer |= val;
        bits_left += 5;

        if (bits_left >= 8) {
            decoded[index++] = (buffer >> (bits_left - 8)) & 0xFF;
            bits_left -= 8;
        }
    }

    if (output_length) {
        *output_length = index;
    }

    return decoded;
}
