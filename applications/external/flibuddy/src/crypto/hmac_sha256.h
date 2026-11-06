/* HMAC-SHA256 (RFC 2104) on top of fb_sha256. */
#pragma once

#include "sha256.h"

typedef struct {
    FbSha256 inner;
    FbSha256 outer;
} FbHmacSha256;

void fb_hmac_sha256_init(FbHmacSha256* ctx, const uint8_t* key, size_t key_len);
void fb_hmac_sha256_update(FbHmacSha256* ctx, const uint8_t* data, size_t len);
void fb_hmac_sha256_final(FbHmacSha256* ctx, uint8_t out[FB_SHA256_DIGEST_LEN]);

/* One-shot MAC; `key` may be NULL when `key_len` is 0. */
void fb_hmac_sha256(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t data_len,
    uint8_t out[FB_SHA256_DIGEST_LEN]);
