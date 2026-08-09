#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_length;
    uint8_t block[64];
    size_t block_length;
} CkSha256;

void ck_sha256_init(CkSha256* context);
void ck_sha256_update(CkSha256* context, const uint8_t* data, size_t length);
void ck_sha256_final(CkSha256* context, uint8_t digest[32]);
void ck_sha256(const uint8_t* data, size_t length, uint8_t digest[32]);
void ck_secure_zero(void* data, size_t length);
