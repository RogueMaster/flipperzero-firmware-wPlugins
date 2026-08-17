/* Minimal FIPS 180-4 SHA-1 for messages that fit in one padded 512-bit block. */
#include "../../sha1_digest.h"

#include <string.h>

static uint32_t rotate_left(uint32_t value, uint8_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

bool sha1_digest_short(const uint8_t* input, size_t length, uint8_t output[20]) {
    if(!input || !output || length > 55) return false;

    uint8_t block[64] = {0};
    memcpy(block, input, length);
    block[length] = 0x80;
    uint64_t bit_length = (uint64_t)length * 8;
    for(uint8_t index = 0; index < 8; index++) {
        block[63 - index] = (uint8_t)(bit_length >> (index * 8));
    }

    uint32_t words[80];
    for(uint8_t index = 0; index < 16; index++) {
        size_t offset = (size_t)index * 4;
        words[index] = ((uint32_t)block[offset] << 24) |
                       ((uint32_t)block[offset + 1] << 16) |
                       ((uint32_t)block[offset + 2] << 8) | block[offset + 3];
    }
    for(uint8_t index = 16; index < 80; index++) {
        words[index] = rotate_left(
            words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
    }

    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    for(uint8_t index = 0; index < 80; index++) {
        uint32_t function;
        uint32_t constant;
        if(index < 20) {
            function = (b & c) | ((~b) & d);
            constant = 0x5A827999;
        } else if(index < 40) {
            function = b ^ c ^ d;
            constant = 0x6ED9EBA1;
        } else if(index < 60) {
            function = (b & c) | (b & d) | (c & d);
            constant = 0x8F1BBCDC;
        } else {
            function = b ^ c ^ d;
            constant = 0xCA62C1D6;
        }
        uint32_t temporary = rotate_left(a, 5) + function + e + constant + words[index];
        e = d;
        d = c;
        c = rotate_left(b, 30);
        b = a;
        a = temporary;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;

    for(uint8_t index = 0; index < 5; index++) {
        output[index * 4] = (uint8_t)(state[index] >> 24);
        output[index * 4 + 1] = (uint8_t)(state[index] >> 16);
        output[index * 4 + 2] = (uint8_t)(state[index] >> 8);
        output[index * 4 + 3] = (uint8_t)state[index];
    }
    return true;
}

bool sha1_digest_self_test(void) {
    static const uint8_t expected[20] = {
        0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
        0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D,
    };
    uint8_t digest[20];
    return sha1_digest_short((const uint8_t*)"abc", 3, digest) &&
           memcmp(digest, expected, sizeof(expected)) == 0;
}
