/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sha256.h"

typedef struct {
    uint8_t key_block[64];
    uint8_t inner_hash[32];
    uint8_t pad[64];
    ZfSha256Context sha;
} ZfHmacSha256Scratch;

bool zf_hmac_sha256_parts_with_scratch(ZfHmacSha256Scratch *scratch, const uint8_t *key,
                                       size_t key_len, const uint8_t *first, size_t first_size,
                                       const uint8_t *second, size_t second_size, uint8_t out[32]);
bool zf_hmac_sha256_parts(const uint8_t *key, size_t key_len, const uint8_t *first,
                          size_t first_size, const uint8_t *second, size_t second_size,
                          uint8_t out[32]);
bool zf_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t size,
                    uint8_t out[32]);
bool zf_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                    const uint8_t *info, size_t info_len, uint8_t out[32]);
