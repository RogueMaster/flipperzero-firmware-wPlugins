/* SHA-256 (FIPS 180-4). Small, no dynamic allocation. The firmware does not
 * export mbedtls hashing to FAPs, so the app ships its own. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define FB_SHA256_DIGEST_LEN 32
#define FB_SHA256_BLOCK_LEN  64

typedef struct {
    uint32_t state[8];
    uint64_t total_len; /* bytes hashed so far */
    uint8_t block[FB_SHA256_BLOCK_LEN];
    size_t block_len; /* bytes buffered in block */
} FbSha256;

void fb_sha256_init(FbSha256* ctx);
void fb_sha256_update(FbSha256* ctx, const uint8_t* data, size_t len);
void fb_sha256_final(FbSha256* ctx, uint8_t out[FB_SHA256_DIGEST_LEN]);

/* One-shot digest of `len` bytes. */
void fb_sha256(const void* data, size_t len, uint8_t out[FB_SHA256_DIGEST_LEN]);
