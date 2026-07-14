/**
 * @file sha1.c
 * @brief Minimal, self-contained SHA-1 implementation.
 *
 * Derived from the public-domain implementation by Steve Reid <steve@edmweb.com>.
 * This file remains in the public domain.
 */
#include "sha1.h"

#include <string.h>

#define SHA1_ROL(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

static uint32_t sha1_blk(const uint8_t* block, size_t i) {
    return ((uint32_t)block[i * 4 + 0] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
           ((uint32_t)block[i * 4 + 2] << 8) | ((uint32_t)block[i * 4 + 3]);
}

static void sha1_transform(uint32_t state[5], const uint8_t buffer[SHA1_BLOCK_SIZE]) {
    uint32_t w[80];

    for(size_t i = 0; i < 16; i++) {
        w[i] = sha1_blk(buffer, i);
    }
    for(size_t i = 16; i < 80; i++) {
        w[i] = SHA1_ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for(size_t i = 0; i < 80; i++) {
        uint32_t f;
        uint32_t k;
        if(i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if(i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if(i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t temp = SHA1_ROL(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = SHA1_ROL(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void sha1_init(Sha1Context* context) {
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = 0;
    context->count[1] = 0;
}

void sha1_update(Sha1Context* context, const uint8_t* data, size_t len) {
    size_t j = (context->count[0] >> 3) & 63;

    if((context->count[0] += (uint32_t)(len << 3)) < (len << 3)) {
        context->count[1]++;
    }
    context->count[1] += (uint32_t)(len >> 29);

    size_t i;
    if((j + len) > 63) {
        i = 64 - j;
        memcpy(&context->buffer[j], data, i);
        sha1_transform(context->state, context->buffer);
        for(; i + 63 < len; i += 64) {
            sha1_transform(context->state, &data[i]);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(&context->buffer[j], &data[i], len - i);
}

void sha1_final(Sha1Context* context, uint8_t digest[SHA1_DIGEST_SIZE]) {
    uint8_t finalcount[8];
    for(size_t i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((context->count[(i >= 4) ? 0 : 1] >> ((3 - (i & 3)) * 8)) & 255);
    }

    const uint8_t c = 0x80;
    sha1_update(context, &c, 1);
    while((context->count[0] & 504) != 448) {
        const uint8_t zero = 0x00;
        sha1_update(context, &zero, 1);
    }
    sha1_update(context, finalcount, 8);

    for(size_t i = 0; i < SHA1_DIGEST_SIZE; i++) {
        digest[i] = (uint8_t)((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }

    memset(context, 0, sizeof(*context));
}

void sha1(const uint8_t* data, size_t len, uint8_t digest[SHA1_DIGEST_SIZE]) {
    Sha1Context context;
    sha1_init(&context);
    sha1_update(&context, data, len);
    sha1_final(&context, digest);
}
