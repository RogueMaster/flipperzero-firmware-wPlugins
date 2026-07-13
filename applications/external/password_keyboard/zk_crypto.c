#include "zk_crypto.h"

#include <furi_hal.h>
#include <string.h>

/* Small keyed BLAKE2s implementation. It is used both as a KDF and to build an
 * authenticated stream cipher. The device UID binds vault files to one Flipper.
 * This protects against casual SD-card inspection, but is not a secure enclave. */

typedef struct {
    uint32_t h[8];
    uint32_t t[2];
    uint8_t buffer[64];
    size_t used;
    size_t out_length;
} ZkBlake2s;

static const uint32_t zk_blake_iv[8] = {
    0x6A09E667,
    0xBB67AE85,
    0x3C6EF372,
    0xA54FF53A,
    0x510E527F,
    0x9B05688C,
    0x1F83D9AB,
    0x5BE0CD19,
};

static const uint8_t zk_blake_sigma[10][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
    {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
    {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
    {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
};

static uint32_t zk_rotr32(uint32_t value, uint8_t bits) {
    return (value >> bits) | (value << (32 - bits));
}

static uint32_t zk_load32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void zk_store32(uint8_t* p, uint32_t value) {
    p[0] = value;
    p[1] = value >> 8;
    p[2] = value >> 16;
    p[3] = value >> 24;
}

#define ZK_G(a, b, c, d, x, y)    \
    do {                          \
        a = a + b + x;            \
        d = zk_rotr32(d ^ a, 16); \
        c = c + d;                \
        b = zk_rotr32(b ^ c, 12); \
        a = a + b + y;            \
        d = zk_rotr32(d ^ a, 8);  \
        c = c + d;                \
        b = zk_rotr32(b ^ c, 7);  \
    } while(0)

static void zk_blake_compress(ZkBlake2s* state, const uint8_t block[64], bool last) {
    uint32_t m[16];
    uint32_t v[16];
    for(size_t i = 0; i < 16; i++)
        m[i] = zk_load32(block + i * 4);
    for(size_t i = 0; i < 8; i++) {
        v[i] = state->h[i];
        v[i + 8] = zk_blake_iv[i];
    }
    v[12] ^= state->t[0];
    v[13] ^= state->t[1];
    if(last) v[14] = ~v[14];
    for(size_t round = 0; round < 10; round++) {
        const uint8_t* s = zk_blake_sigma[round];
        ZK_G(v[0], v[4], v[8], v[12], m[s[0]], m[s[1]]);
        ZK_G(v[1], v[5], v[9], v[13], m[s[2]], m[s[3]]);
        ZK_G(v[2], v[6], v[10], v[14], m[s[4]], m[s[5]]);
        ZK_G(v[3], v[7], v[11], v[15], m[s[6]], m[s[7]]);
        ZK_G(v[0], v[5], v[10], v[15], m[s[8]], m[s[9]]);
        ZK_G(v[1], v[6], v[11], v[12], m[s[10]], m[s[11]]);
        ZK_G(v[2], v[7], v[8], v[13], m[s[12]], m[s[13]]);
        ZK_G(v[3], v[4], v[9], v[14], m[s[14]], m[s[15]]);
    }
    for(size_t i = 0; i < 8; i++)
        state->h[i] ^= v[i] ^ v[i + 8];
}

static void
    zk_blake_init(ZkBlake2s* state, size_t out_length, const uint8_t* key, size_t key_length) {
    memset(state, 0, sizeof(*state));
    memcpy(state->h, zk_blake_iv, sizeof(state->h));
    state->out_length = out_length;
    state->h[0] ^= 0x01010000U ^ ((uint32_t)key_length << 8) ^ (uint32_t)out_length;
    if(key_length) {
        memcpy(state->buffer, key, key_length);
        state->used = 64;
    }
}

static void zk_blake_increment(ZkBlake2s* state, uint32_t count) {
    state->t[0] += count;
    if(state->t[0] < count) state->t[1]++;
}

static void zk_blake_update(ZkBlake2s* state, const uint8_t* data, size_t length) {
    while(length) {
        if(state->used == 64) {
            zk_blake_increment(state, 64);
            zk_blake_compress(state, state->buffer, false);
            state->used = 0;
        }
        size_t take = 64 - state->used;
        if(take > length) take = length;
        memcpy(state->buffer + state->used, data, take);
        state->used += take;
        data += take;
        length -= take;
    }
}

static void zk_blake_final(ZkBlake2s* state, uint8_t* output) {
    zk_blake_increment(state, state->used);
    memset(state->buffer + state->used, 0, 64 - state->used);
    zk_blake_compress(state, state->buffer, true);
    uint8_t full[32];
    for(size_t i = 0; i < 8; i++)
        zk_store32(full + i * 4, state->h[i]);
    memcpy(output, full, state->out_length);
    zk_crypto_wipe(full, sizeof(full));
    zk_crypto_wipe(state, sizeof(*state));
}

static void zk_blake_hash(
    uint8_t* output,
    size_t out_length,
    const uint8_t* key,
    size_t key_length,
    const uint8_t* first,
    size_t first_length,
    const uint8_t* second,
    size_t second_length) {
    ZkBlake2s state;
    zk_blake_init(&state, out_length, key, key_length);
    if(first_length) zk_blake_update(&state, first, first_length);
    if(second_length) zk_blake_update(&state, second, second_length);
    zk_blake_final(&state, output);
}

void zk_crypto_derive_device_key(uint8_t key[ZK_KEY_SIZE]) {
    static const uint8_t domain[] = "PasswordKeyboard vault key v1";
    const uint8_t* uid = furi_hal_version_uid();
    const size_t uid_length = furi_hal_version_uid_size();
    zk_blake_hash(key, ZK_KEY_SIZE, NULL, 0, domain, sizeof(domain) - 1, uid, uid_length);
}

static void zk_crypto_stream(
    const uint8_t key[ZK_KEY_SIZE],
    const uint8_t nonce[ZK_NONCE_SIZE],
    const uint8_t* input,
    size_t length,
    uint8_t* output) {
    uint8_t message[ZK_NONCE_SIZE + 4];
    uint8_t block[32];
    memcpy(message, nonce, ZK_NONCE_SIZE);
    uint32_t counter = 0;
    size_t offset = 0;
    while(offset < length) {
        zk_store32(message + ZK_NONCE_SIZE, counter++);
        zk_blake_hash(block, sizeof(block), key, ZK_KEY_SIZE, message, sizeof(message), NULL, 0);
        size_t take = length - offset;
        if(take > sizeof(block)) take = sizeof(block);
        for(size_t i = 0; i < take; i++)
            output[offset + i] = input[offset + i] ^ block[i];
        offset += take;
    }
    zk_crypto_wipe(block, sizeof(block));
    zk_crypto_wipe(message, sizeof(message));
}

static void zk_crypto_tag(
    const uint8_t key[ZK_KEY_SIZE],
    const uint8_t nonce[ZK_NONCE_SIZE],
    const uint8_t* cipher,
    size_t length,
    uint8_t tag[ZK_TAG_SIZE]) {
    ZkBlake2s state;
    uint8_t encoded_length[4];
    zk_store32(encoded_length, (uint32_t)length);
    zk_blake_init(&state, ZK_TAG_SIZE, key, ZK_KEY_SIZE);
    zk_blake_update(&state, nonce, ZK_NONCE_SIZE);
    zk_blake_update(&state, encoded_length, sizeof(encoded_length));
    zk_blake_update(&state, cipher, length);
    zk_blake_final(&state, tag);
}

void zk_crypto_seal(
    const uint8_t key[ZK_KEY_SIZE],
    const uint8_t nonce[ZK_NONCE_SIZE],
    const uint8_t* plain,
    size_t length,
    uint8_t* cipher,
    uint8_t tag[ZK_TAG_SIZE]) {
    zk_crypto_stream(key, nonce, plain, length, cipher);
    zk_crypto_tag(key, nonce, cipher, length, tag);
}

bool zk_crypto_open(
    const uint8_t key[ZK_KEY_SIZE],
    const uint8_t nonce[ZK_NONCE_SIZE],
    const uint8_t* cipher,
    size_t length,
    const uint8_t tag[ZK_TAG_SIZE],
    uint8_t* plain) {
    uint8_t expected[ZK_TAG_SIZE];
    zk_crypto_tag(key, nonce, cipher, length, expected);
    uint8_t difference = 0;
    for(size_t i = 0; i < sizeof(expected); i++)
        difference |= expected[i] ^ tag[i];
    zk_crypto_wipe(expected, sizeof(expected));
    if(difference) return false;
    zk_crypto_stream(key, nonce, cipher, length, plain);
    return true;
}

void zk_crypto_wipe(void* data, size_t length) {
    volatile uint8_t* bytes = data;
    while(length--)
        *bytes++ = 0;
}
