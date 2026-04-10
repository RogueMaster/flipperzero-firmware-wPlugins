/**
 * tiny-AES-c — AES-128 ECB and CBC
 * Public domain by Odzhan / kokke, trimmed to AES-128 only.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define AES_BLOCKLEN 16
#define AES_KEYLEN   16
#define AES_keyExpSize 176

struct AES_ctx {
    uint8_t RoundKey[AES_keyExpSize];
    uint8_t Iv[AES_BLOCKLEN];
};

void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key);
void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv);
void AES_ctx_set_iv(struct AES_ctx* ctx, const uint8_t* iv);

/** ECB: single block in-place */
void AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf);
void AES_ECB_decrypt(const struct AES_ctx* ctx, uint8_t* buf);

/** CBC: buf length must be multiple of AES_BLOCKLEN */
void AES_CBC_encrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);
void AES_CBC_decrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);
