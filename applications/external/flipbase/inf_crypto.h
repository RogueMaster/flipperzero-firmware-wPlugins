/*
 * Minimal SHA-1 and AES-128 (ECB, single block) used for Disney Infinity
 * figure files. Self-contained so it builds on the Flipper Zero and on a PC.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[5];
    uint64_t length; /* bytes processed so far */
    uint8_t buffer[64];
    uint32_t buffer_len;
} InfSha1;

void inf_sha1_init(InfSha1* ctx);
void inf_sha1_update(InfSha1* ctx, const uint8_t* data, size_t len);
void inf_sha1_final(InfSha1* ctx, uint8_t out[20]);

typedef struct {
    uint8_t round_keys[176];
} InfAes128;

void inf_aes128_set_key(InfAes128* ctx, const uint8_t key[16]);
void inf_aes128_encrypt_block(const InfAes128* ctx, const uint8_t in[16], uint8_t out[16]);
void inf_aes128_decrypt_block(const InfAes128* ctx, const uint8_t in[16], uint8_t out[16]);

#ifdef __cplusplus
}
#endif
