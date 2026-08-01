#include "ck42x_sha256.h"

#include <string.h>

static const uint32_t ck_sha256_k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

static uint32_t rotr(uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32U - bits));
}

static uint32_t get_u32(const uint8_t* data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static void put_u32(uint8_t* data, uint32_t value) {
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void transform(CkSha256* context, const uint8_t block[64]) {
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    for(size_t i = 0; i < 16U; ++i)
        words[i] = get_u32(block + i * 4U);
    for(size_t i = 16U; i < 64U; ++i) {
        uint32_t s0 = rotr(words[i - 15U], 7) ^ rotr(words[i - 15U], 18) ^ (words[i - 15U] >> 3);
        uint32_t s1 = rotr(words[i - 2U], 17) ^ rotr(words[i - 2U], 19) ^ (words[i - 2U] >> 10);
        words[i] = words[i - 16U] + s0 + words[i - 7U] + s1;
    }
    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];
    for(size_t i = 0; i < 64U; ++i) {
        uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + choice + ck_sha256_k[i] + words[i];
        uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
    ck_secure_zero(words, sizeof(words));
}

void ck_secure_zero(void* data, size_t length) {
    volatile uint8_t* bytes = (volatile uint8_t*)data;
    while(length-- != 0U)
        *bytes++ = 0;
}

void ck_sha256_init(CkSha256* context) {
    static const uint32_t initial[8] = {
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U};
    memcpy(context->state, initial, sizeof(initial));
    context->bit_length = 0;
    context->block_length = 0;
}

void ck_sha256_update(CkSha256* context, const uint8_t* data, size_t length) {
    while(length-- != 0U) {
        context->block[context->block_length++] = *data++;
        if(context->block_length == sizeof(context->block)) {
            transform(context, context->block);
            context->bit_length += 512U;
            context->block_length = 0;
        }
    }
}

void ck_sha256_final(CkSha256* context, uint8_t digest[32]) {
    size_t used = context->block_length;
    context->bit_length += (uint64_t)used * 8U;
    context->block[used++] = 0x80U;
    if(used > 56U) {
        memset(context->block + used, 0, 64U - used);
        transform(context, context->block);
        used = 0;
    }
    memset(context->block + used, 0, 56U - used);
    for(size_t i = 0; i < 8U; ++i)
        context->block[63U - i] = (uint8_t)(context->bit_length >> (i * 8U));
    transform(context, context->block);
    for(size_t i = 0; i < 8U; ++i)
        put_u32(digest + i * 4U, context->state[i]);
    ck_secure_zero(context, sizeof(*context));
}

void ck_sha256(const uint8_t* data, size_t length, uint8_t digest[32]) {
    CkSha256 context;
    ck_sha256_init(&context);
    ck_sha256_update(&context, data, length);
    ck_sha256_final(&context, digest);
}
