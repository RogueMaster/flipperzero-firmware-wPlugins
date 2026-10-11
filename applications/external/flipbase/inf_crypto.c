/*
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "inf_crypto.h"

#include <string.h>

/* ------------------------------------------------------------------ SHA-1 */

#define ROL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_block(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for(int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[4 * i] << 24) | ((uint32_t)block[4 * i + 1] << 16) |
               ((uint32_t)block[4 * i + 2] << 8) | (uint32_t)block[4 * i + 3];
    }
    for(int i = 16; i < 80; i++) {
        w[i] = ROL32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for(int i = 0; i < 80; i++) {
        uint32_t f, k;
        if(i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        } else if(i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if(i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t temp = ROL32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROL32(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void inf_sha1_init(InfSha1* ctx) {
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u;
    ctx->length = 0;
    ctx->buffer_len = 0;
}

void inf_sha1_update(InfSha1* ctx, const uint8_t* data, size_t len) {
    ctx->length += len;
    while(len > 0) {
        size_t n = 64 - ctx->buffer_len;
        if(n > len) n = len;
        memcpy(ctx->buffer + ctx->buffer_len, data, n);
        ctx->buffer_len += (uint32_t)n;
        data += n;
        len -= n;
        if(ctx->buffer_len == 64) {
            sha1_block(ctx->state, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

void inf_sha1_final(InfSha1* ctx, uint8_t out[20]) {
    const uint64_t bits = ctx->length * 8u;
    const uint8_t pad_one = 0x80;
    const uint8_t pad_zero = 0x00;

    inf_sha1_update(ctx, &pad_one, 1);
    while(ctx->buffer_len != 56) {
        inf_sha1_update(ctx, &pad_zero, 1);
    }
    uint8_t len_bytes[8];
    for(int i = 0; i < 8; i++) {
        len_bytes[i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    inf_sha1_update(ctx, len_bytes, 8);

    for(int i = 0; i < 5; i++) {
        out[4 * i] = (uint8_t)(ctx->state[i] >> 24);
        out[4 * i + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[4 * i + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[4 * i + 3] = (uint8_t)(ctx->state[i]);
    }
}

/* -------------------------------------------------------------- AES-128 */

static uint8_t g_sbox[256];
static uint8_t g_inv_sbox[256];
static int g_tables_ready = 0;

static uint8_t rotl8(uint8_t x, int n) {
    return (uint8_t)((x << n) | (x >> (8 - n)));
}

static uint8_t xtime(uint8_t x) {
    return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1B : 0x00));
}

static uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t result = 0;
    while(b) {
        if(b & 1) result ^= a;
        a = xtime(a);
        b >>= 1;
    }
    return result;
}

/* Build the S-box from its definition (inverse in GF(2^8) + affine map). */
static void build_tables(void) {
    uint8_t p = 1, q = 1;
    do {
        /* p *= 3 */
        p = (uint8_t)(p ^ (uint8_t)(p << 1) ^ ((p & 0x80) ? 0x1B : 0x00));
        /* q /= 3 */
        q = (uint8_t)(q ^ (uint8_t)(q << 1));
        q = (uint8_t)(q ^ (uint8_t)(q << 2));
        q = (uint8_t)(q ^ (uint8_t)(q << 4));
        if(q & 0x80) q ^= 0x09;
        uint8_t x = (uint8_t)(q ^ rotl8(q, 1) ^ rotl8(q, 2) ^ rotl8(q, 3) ^ rotl8(q, 4));
        g_sbox[p] = (uint8_t)(x ^ 0x63);
    } while(p != 1);
    g_sbox[0] = 0x63;
    for(int i = 0; i < 256; i++) {
        g_inv_sbox[g_sbox[i]] = (uint8_t)i;
    }
    g_tables_ready = 1;
}

void inf_aes128_set_key(InfAes128* ctx, const uint8_t key[16]) {
    if(!g_tables_ready) build_tables();

    uint8_t* rk = ctx->round_keys;
    memcpy(rk, key, 16);
    uint8_t rcon = 1;
    for(int i = 16; i < 176; i += 4) {
        uint8_t t[4] = {rk[i - 4], rk[i - 3], rk[i - 2], rk[i - 1]};
        if(i % 16 == 0) {
            const uint8_t t0 = t[0];
            t[0] = (uint8_t)(g_sbox[t[1]] ^ rcon);
            t[1] = g_sbox[t[2]];
            t[2] = g_sbox[t[3]];
            t[3] = g_sbox[t0];
            rcon = xtime(rcon);
        }
        for(int j = 0; j < 4; j++) {
            rk[i + j] = (uint8_t)(rk[i - 16 + j] ^ t[j]);
        }
    }
}

static void add_round_key(uint8_t s[16], const uint8_t* rk) {
    for(int i = 0; i < 16; i++)
        s[i] ^= rk[i];
}

static void sub_bytes(uint8_t s[16]) {
    for(int i = 0; i < 16; i++)
        s[i] = g_sbox[s[i]];
}

static void inv_sub_bytes(uint8_t s[16]) {
    for(int i = 0; i < 16; i++)
        s[i] = g_inv_sbox[s[i]];
}

/* State layout is column-major: s[row + 4 * column]. */
static void shift_rows(uint8_t s[16]) {
    uint8_t t[16];
    for(int c = 0; c < 4; c++) {
        for(int r = 0; r < 4; r++) {
            t[r + 4 * c] = s[r + 4 * ((c + r) % 4)];
        }
    }
    memcpy(s, t, 16);
}

static void inv_shift_rows(uint8_t s[16]) {
    uint8_t t[16];
    for(int c = 0; c < 4; c++) {
        for(int r = 0; r < 4; r++) {
            t[r + 4 * ((c + r) % 4)] = s[r + 4 * c];
        }
    }
    memcpy(s, t, 16);
}

static void mix_columns(uint8_t s[16]) {
    for(int c = 0; c < 4; c++) {
        uint8_t* col = s + 4 * c;
        const uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
        const uint8_t t = (uint8_t)(a0 ^ a1 ^ a2 ^ a3);
        col[0] = (uint8_t)(a0 ^ t ^ xtime((uint8_t)(a0 ^ a1)));
        col[1] = (uint8_t)(a1 ^ t ^ xtime((uint8_t)(a1 ^ a2)));
        col[2] = (uint8_t)(a2 ^ t ^ xtime((uint8_t)(a2 ^ a3)));
        col[3] = (uint8_t)(a3 ^ t ^ xtime((uint8_t)(a3 ^ a0)));
    }
}

static void inv_mix_columns(uint8_t s[16]) {
    for(int c = 0; c < 4; c++) {
        uint8_t* col = s + 4 * c;
        const uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
        col[0] = (uint8_t)(gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9));
        col[1] = (uint8_t)(gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13));
        col[2] = (uint8_t)(gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11));
        col[3] = (uint8_t)(gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14));
    }
}

void inf_aes128_encrypt_block(const InfAes128* ctx, const uint8_t in[16], uint8_t out[16]) {
    uint8_t s[16];
    memcpy(s, in, 16);
    add_round_key(s, ctx->round_keys);
    for(int round = 1; round < 10; round++) {
        sub_bytes(s);
        shift_rows(s);
        mix_columns(s);
        add_round_key(s, ctx->round_keys + 16 * round);
    }
    sub_bytes(s);
    shift_rows(s);
    add_round_key(s, ctx->round_keys + 160);
    memcpy(out, s, 16);
}

void inf_aes128_decrypt_block(const InfAes128* ctx, const uint8_t in[16], uint8_t out[16]) {
    uint8_t s[16];
    memcpy(s, in, 16);
    add_round_key(s, ctx->round_keys + 160);
    for(int round = 9; round >= 1; round--) {
        inv_shift_rows(s);
        inv_sub_bytes(s);
        add_round_key(s, ctx->round_keys + 16 * round);
        inv_mix_columns(s);
    }
    inv_shift_rows(s);
    inv_sub_bytes(s);
    add_round_key(s, ctx->round_keys);
    memcpy(out, s, 16);
}
