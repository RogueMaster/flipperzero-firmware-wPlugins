#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} Sha1Context;

void sha1_init(Sha1Context* ctx);
void sha1_update(Sha1Context* ctx, const uint8_t* data, size_t len);
void sha1_finalize(Sha1Context* ctx, uint8_t hash[20]);
void sha1_to_hex(const uint8_t hash[20], char* output);