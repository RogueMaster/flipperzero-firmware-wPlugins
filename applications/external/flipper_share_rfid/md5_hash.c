#include "md5_hash.h"

#include <string.h>

// Per-round left-rotate amounts (RFC 1321)
static const uint8_t MD5_SHIFTS[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, //
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, //
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, //
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

// K[i] = floor(abs(sin(i + 1)) * 2^32) (RFC 1321)
static const uint32_t MD5_K[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, //
    0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u, //
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, //
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u, //
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, //
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u, //
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, //
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au, //
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, //
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, //
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, //
    0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u, //
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, //
    0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u, //
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, //
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};

static uint32_t md5_rotl(uint32_t x, uint32_t s) {
    return (x << s) | (x >> (32u - s));
}

static void md5_process_block(Md5Context* ctx, const uint8_t block[64]) {
    uint32_t m[16];
    for(size_t i = 0; i < 16; i++) {
        m[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) | ((uint32_t)block[i * 4 + 3] << 24);
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];

    for(uint32_t i = 0; i < 64; i++) {
        uint32_t f, g;
        if(i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if(i < 32) {
            f = (d & b) | (~d & c);
            g = (5u * i + 1u) & 15u;
        } else if(i < 48) {
            f = b ^ c ^ d;
            g = (3u * i + 5u) & 15u;
        } else {
            f = c ^ (b | ~d);
            g = (7u * i) & 15u;
        }
        uint32_t tmp = d;
        d = c;
        c = b;
        b = b + md5_rotl(a + f + MD5_K[i] + m[g], MD5_SHIFTS[i]);
        a = tmp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

void md5_hash_init(Md5Context* ctx) {
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xefcdab89u;
    ctx->state[2] = 0x98badcfeu;
    ctx->state[3] = 0x10325476u;
    ctx->total_len = 0;
    ctx->buffer_len = 0;
}

void md5_hash_update(Md5Context* ctx, const void* data, size_t len) {
    const uint8_t* p = data;
    ctx->total_len += len;

    if(ctx->buffer_len) {
        size_t take = 64 - ctx->buffer_len;
        if(take > len) take = len;
        memcpy(ctx->buffer + ctx->buffer_len, p, take);
        ctx->buffer_len += take;
        p += take;
        len -= take;
        if(ctx->buffer_len == 64) {
            md5_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
    while(len >= 64) {
        md5_process_block(ctx, p);
        p += 64;
        len -= 64;
    }
    if(len) {
        memcpy(ctx->buffer, p, len);
        ctx->buffer_len = len;
    }
}

void md5_hash_finish(Md5Context* ctx, uint8_t digest[16]) {
    // Bit length must be captured before padding is fed through update
    uint64_t bit_len = ctx->total_len * 8u;
    uint8_t tail[8];
    for(size_t i = 0; i < 8; i++) {
        tail[i] = (uint8_t)(bit_len >> (8u * i));
    }

    static const uint8_t padding[64] = {0x80};
    size_t pad_len = (ctx->buffer_len < 56) ? (56 - ctx->buffer_len) : (120 - ctx->buffer_len);
    md5_hash_update(ctx, padding, pad_len);
    md5_hash_update(ctx, tail, sizeof(tail));

    for(size_t i = 0; i < 4; i++) {
        digest[i * 4] = (uint8_t)(ctx->state[i]);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] >> 24);
    }
}
