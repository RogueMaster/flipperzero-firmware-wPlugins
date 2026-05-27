/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[64];
    size_t buffer_len;
} ZfSha256Context;

void zf_sha256_init(ZfSha256Context *ctx);
void zf_sha256_update(ZfSha256Context *ctx, const uint8_t *data, size_t size);
void zf_sha256_finish(ZfSha256Context *ctx, uint8_t out[32]);
