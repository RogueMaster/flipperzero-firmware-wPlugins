#pragma once
#include <stdint.h>
#include <stddef.h>

#define BLAKE2S_OUTLEN 32

typedef struct {
    uint32_t h[8];
    uint32_t t[2];
    uint32_t f[2];
    uint8_t buf[64];
    size_t buflen;
    size_t outlen;
} Blake2sContext;

void blake2s_init(Blake2sContext* ctx);
void blake2s_update(Blake2sContext* ctx, const uint8_t* input, size_t inlen);
void blake2s_finalize(Blake2sContext* ctx, uint8_t* out);
void blake2s_to_hex(const uint8_t hash[BLAKE2S_OUTLEN], char* output);