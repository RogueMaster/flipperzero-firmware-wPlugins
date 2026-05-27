/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "sha256.h"

#include <string.h>

static const uint32_t zf_sha256_k[64] = {
    0x428A2F98UL, 0x71374491UL, 0xB5C0FBCFUL, 0xE9B5DBA5UL, 0x3956C25BUL, 0x59F111F1UL,
    0x923F82A4UL, 0xAB1C5ED5UL, 0xD807AA98UL, 0x12835B01UL, 0x243185BEUL, 0x550C7DC3UL,
    0x72BE5D74UL, 0x80DEB1FEUL, 0x9BDC06A7UL, 0xC19BF174UL, 0xE49B69C1UL, 0xEFBE4786UL,
    0x0FC19DC6UL, 0x240CA1CCUL, 0x2DE92C6FUL, 0x4A7484AAUL, 0x5CB0A9DCUL, 0x76F988DAUL,
    0x983E5152UL, 0xA831C66DUL, 0xB00327C8UL, 0xBF597FC7UL, 0xC6E00BF3UL, 0xD5A79147UL,
    0x06CA6351UL, 0x14292967UL, 0x27B70A85UL, 0x2E1B2138UL, 0x4D2C6DFCUL, 0x53380D13UL,
    0x650A7354UL, 0x766A0ABBUL, 0x81C2C92EUL, 0x92722C85UL, 0xA2BFE8A1UL, 0xA81A664BUL,
    0xC24B8B70UL, 0xC76C51A3UL, 0xD192E819UL, 0xD6990624UL, 0xF40E3585UL, 0x106AA070UL,
    0x19A4C116UL, 0x1E376C08UL, 0x2748774CUL, 0x34B0BCB5UL, 0x391C0CB3UL, 0x4ED8AA4AUL,
    0x5B9CCA4FUL, 0x682E6FF3UL, 0x748F82EEUL, 0x78A5636FUL, 0x84C87814UL, 0x8CC70208UL,
    0x90BEFFFAUL, 0xA4506CEBUL, 0xBEF9A3F7UL, 0xC67178F2UL,
};

static void zf_sha256_zero(void *data, size_t size) {
    volatile uint8_t *ptr = data;

    while (size-- > 0U) {
        *ptr++ = 0;
    }
}

static uint32_t zf_sha256_rotr(uint32_t value, uint8_t shift) {
    return (value >> shift) | (value << (32U - shift));
}

static uint32_t zf_sha256_load_be32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static void zf_sha256_store_be32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static void zf_sha256_store_be64(uint8_t *out, uint64_t value) {
    for (size_t i = 0; i < 8U; ++i) {
        out[i] = (uint8_t)(value >> (56U - (i * 8U)));
    }
}

static void zf_sha256_transform(ZfSha256Context *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    for (size_t i = 0; i < 16U; ++i) {
        w[i] = zf_sha256_load_be32(block + (i * 4U));
    }
    for (size_t i = 16U; i < 64U; ++i) {
        uint32_t s0 =
            zf_sha256_rotr(w[i - 15U], 7) ^ zf_sha256_rotr(w[i - 15U], 18) ^ (w[i - 15U] >> 3);
        uint32_t s1 =
            zf_sha256_rotr(w[i - 2U], 17) ^ zf_sha256_rotr(w[i - 2U], 19) ^ (w[i - 2U] >> 10);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }

    for (size_t i = 0; i < 64U; ++i) {
        uint32_t s1 = zf_sha256_rotr(e, 6) ^ zf_sha256_rotr(e, 11) ^ zf_sha256_rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + s1 + ch + zf_sha256_k[i] + w[i];
        uint32_t s0 = zf_sha256_rotr(a, 2) ^ zf_sha256_rotr(a, 13) ^ zf_sha256_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
    zf_sha256_zero(w, sizeof(w));
}

void zf_sha256_init(ZfSha256Context *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6A09E667UL;
    ctx->state[1] = 0xBB67AE85UL;
    ctx->state[2] = 0x3C6EF372UL;
    ctx->state[3] = 0xA54FF53AUL;
    ctx->state[4] = 0x510E527FUL;
    ctx->state[5] = 0x9B05688CUL;
    ctx->state[6] = 0x1F83D9ABUL;
    ctx->state[7] = 0x5BE0CD19UL;
}

void zf_sha256_update(ZfSha256Context *ctx, const uint8_t *data, size_t size) {
    if (!ctx || (!data && size > 0U)) {
        return;
    }

    ctx->bit_count += ((uint64_t)size) * 8U;
    while (size > 0U) {
        size_t chunk = sizeof(ctx->buffer) - ctx->buffer_len;
        if (chunk > size) {
            chunk = size;
        }
        memcpy(ctx->buffer + ctx->buffer_len, data, chunk);
        ctx->buffer_len += chunk;
        data += chunk;
        size -= chunk;

        if (ctx->buffer_len == sizeof(ctx->buffer)) {
            zf_sha256_transform(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

void zf_sha256_finish(ZfSha256Context *ctx, uint8_t out[32]) {
    uint8_t tail[64] = {0};
    uint64_t message_bits = ctx->bit_count;

    tail[0] = 0x80;
    if (ctx->buffer_len > 55U) {
        zf_sha256_update(ctx, tail, sizeof(ctx->buffer) - ctx->buffer_len);
        memset(tail, 0, sizeof(tail));
    }
    zf_sha256_update(ctx, tail, 56U - ctx->buffer_len);
    zf_sha256_store_be64(ctx->buffer + 56U, message_bits);
    zf_sha256_transform(ctx, ctx->buffer);

    for (size_t i = 0; i < 8U; ++i) {
        zf_sha256_store_be32(out + (i * 4U), ctx->state[i]);
    }
    zf_sha256_zero(ctx, sizeof(*ctx));
    zf_sha256_zero(tail, sizeof(tail));
}
