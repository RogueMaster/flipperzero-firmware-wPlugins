#pragma once

#include <stdint.h>
#include <stddef.h>

// Minimal incremental MD5 (RFC 1321). Vendored because the app SDK exposes
// only the blocking toolbox md5_calc_file() and no incremental hash API,
// which is needed to report progress and honor cancellation between chunks.

typedef struct {
    uint32_t state[4];
    uint64_t total_len; // bytes fed so far
    uint8_t buffer[64];
    size_t buffer_len;
} Md5Context;

void md5_hash_init(Md5Context* ctx);
void md5_hash_update(Md5Context* ctx, const void* data, size_t len);
void md5_hash_finish(Md5Context* ctx, uint8_t digest[16]);
