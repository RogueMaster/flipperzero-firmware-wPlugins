#include "hmac_sha256.h"

#include <string.h>

void fb_hmac_sha256_init(FbHmacSha256* ctx, const uint8_t* key, size_t key_len) {
    uint8_t block[FB_SHA256_BLOCK_LEN] = {0};
    if(key_len > FB_SHA256_BLOCK_LEN) {
        fb_sha256(key, key_len, block);
    } else if(key_len > 0) {
        memcpy(block, key, key_len);
    }

    uint8_t pad[FB_SHA256_BLOCK_LEN];
    for(size_t i = 0; i < FB_SHA256_BLOCK_LEN; i++)
        pad[i] = block[i] ^ 0x36;
    fb_sha256_init(&ctx->inner);
    fb_sha256_update(&ctx->inner, pad, sizeof(pad));

    for(size_t i = 0; i < FB_SHA256_BLOCK_LEN; i++)
        pad[i] = block[i] ^ 0x5c;
    fb_sha256_init(&ctx->outer);
    fb_sha256_update(&ctx->outer, pad, sizeof(pad));

    memset(block, 0, sizeof(block));
    memset(pad, 0, sizeof(pad));
}

void fb_hmac_sha256_update(FbHmacSha256* ctx, const uint8_t* data, size_t len) {
    fb_sha256_update(&ctx->inner, data, len);
}

void fb_hmac_sha256_final(FbHmacSha256* ctx, uint8_t out[FB_SHA256_DIGEST_LEN]) {
    uint8_t inner[FB_SHA256_DIGEST_LEN];
    fb_sha256_final(&ctx->inner, inner);
    fb_sha256_update(&ctx->outer, inner, sizeof(inner));
    fb_sha256_final(&ctx->outer, out);
    memset(inner, 0, sizeof(inner));
}

void fb_hmac_sha256(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t data_len,
    uint8_t out[FB_SHA256_DIGEST_LEN]) {
    FbHmacSha256 ctx;
    fb_hmac_sha256_init(&ctx, key, key_len);
    fb_hmac_sha256_update(&ctx, data, data_len);
    fb_hmac_sha256_final(&ctx, out);
}
