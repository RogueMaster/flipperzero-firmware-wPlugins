#include "blake2.h"
#include <stdio.h>
#include <string.h>

#define ROTR32(x, y) (((x) >> (y)) ^ ((x) << (32 - (y))))
#define B2S_BLOCKBYTES 64

static const uint32_t blake2s_iv[8] = {
    0x6A09E667U, 0xBB67AE85U,
    0x3C6EF372U, 0xA54FF53AU,
    0x510E527FU, 0x9B05688CU,
    0x1F83D9ABU, 0x5BE0CD19U
};

static const uint8_t sigma[10][16] = {
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 },
  {14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3 },
  {11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4 },
  { 7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8 },
  { 9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13 },
  { 2,12, 6,10, 0,11, 8, 3, 4,13, 7, 5,15,14, 1, 9 },
  {12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11 },
  {13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10 },
  { 6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5 },
  {10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0 }
};

static void blake2s_compress(Blake2sContext* ctx, const uint8_t block[64]) {
    uint32_t m[16];
    uint32_t v[16];

    for (int i = 0; i < 16; ++i) {
        m[i] = ((uint32_t)block[i * 4 + 0] << 0) |
               ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) |
               ((uint32_t)block[i * 4 + 3] << 24);
    }

    for (int i = 0; i < 8; ++i) v[i] = ctx->h[i];
    for (int i = 0; i < 8; ++i) v[i + 8] = blake2s_iv[i];

    v[12] ^= ctx->t[0];
    v[13] ^= ctx->t[1];
    v[14] ^= ctx->f[0];
    v[15] ^= ctx->f[1];

    #define G(a,b,c,d,x,y)              \
        a = a + b + x; d = ROTR32(d ^ a, 16); \
        c = c + d; b = ROTR32(b ^ c, 12);     \
        a = a + b + y; d = ROTR32(d ^ a, 8);  \
        c = c + d; b = ROTR32(b ^ c, 7);

    for (int i = 0; i < 10; ++i) {
        const uint8_t* s = sigma[i];
        G(v[0], v[4], v[8], v[12], m[s[0]], m[s[1]]);
        G(v[1], v[5], v[9], v[13], m[s[2]], m[s[3]]);
        G(v[2], v[6], v[10], v[14], m[s[4]], m[s[5]]);
        G(v[3], v[7], v[11], v[15], m[s[6]], m[s[7]]);
        G(v[0], v[5], v[10], v[15], m[s[8]], m[s[9]]);
        G(v[1], v[6], v[11], v[12], m[s[10]], m[s[11]]);
        G(v[2], v[7], v[8], v[13], m[s[12]], m[s[13]]);
        G(v[3], v[4], v[9], v[14], m[s[14]], m[s[15]]);
    }

    #undef G

    for (int i = 0; i < 8; ++i) {
        ctx->h[i] ^= v[i] ^ v[i + 8];
    }
}

void blake2s_init(Blake2sContext* ctx) {
    memset(ctx, 0, sizeof(Blake2sContext));
    ctx->outlen = BLAKE2S_OUTLEN;

    for (int i = 0; i < 8; ++i)
        ctx->h[i] = blake2s_iv[i];
    ctx->h[0] ^= 0x01010000 | ctx->outlen;  // parameter block
}

void blake2s_update(Blake2sContext* ctx, const uint8_t* in, size_t inlen) {
    while (inlen > 0) {
        size_t left = ctx->buflen;
        size_t fill = 64 - left;

        if (inlen > fill) {
            memcpy(ctx->buf + left, in, fill);
            ctx->t[0] += 64;
            if (ctx->t[0] < 64) ctx->t[1]++;
            blake2s_compress(ctx, ctx->buf);
            ctx->buflen = 0;
            in += fill;
            inlen -= fill;
        } else {
            memcpy(ctx->buf + left, in, inlen);
            ctx->buflen += inlen;
            return;
        }
    }
}

void blake2s_finalize(Blake2sContext* ctx, uint8_t* out) {
    ctx->t[0] += ctx->buflen;
    if (ctx->t[0] < ctx->buflen) ctx->t[1]++;
    ctx->f[0] = 0xFFFFFFFF;

    while (ctx->buflen < 64)
        ctx->buf[ctx->buflen++] = 0;

    blake2s_compress(ctx, ctx->buf);

    for (int i = 0; i < 8; ++i) {
        out[i * 4 + 0] = (ctx->h[i] >> 0) & 0xFF;
        out[i * 4 + 1] = (ctx->h[i] >> 8) & 0xFF;
        out[i * 4 + 2] = (ctx->h[i] >> 16) & 0xFF;
        out[i * 4 + 3] = (ctx->h[i] >> 24) & 0xFF;
    }
}

void blake2s_to_hex(const uint8_t hash[BLAKE2S_OUTLEN], char* output) {
    for (int i = 0; i < BLAKE2S_OUTLEN; ++i) {
        snprintf(output + i * 2, 3, "%02x", hash[i]);
    }
    output[BLAKE2S_OUTLEN * 2] = '\0';
}