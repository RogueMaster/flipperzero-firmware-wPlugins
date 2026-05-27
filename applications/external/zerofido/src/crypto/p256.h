/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../zerofido_types.h"

typedef struct {
    uint8_t private_key[ZF_PRIVATE_KEY_LEN];
    uint8_t public_x[ZF_PUBLIC_KEY_LEN];
    uint8_t public_y[ZF_PUBLIC_KEY_LEN];
} ZfP256KeyAgreementKey;

bool zf_p256_generate_keypair(uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                              uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                              uint8_t public_y[ZF_PUBLIC_KEY_LEN]);
bool zf_p256_private_key_valid(const uint8_t private_key[ZF_PRIVATE_KEY_LEN]);
bool zf_p256_public_key_valid(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                              const uint8_t public_y[ZF_PUBLIC_KEY_LEN]);
bool zf_p256_compute_public_key(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                uint8_t public_y[ZF_PUBLIC_KEY_LEN]);
bool zf_p256_ecdh_raw_secret(const ZfP256KeyAgreementKey *key,
                             const uint8_t peer_x[ZF_PUBLIC_KEY_LEN],
                             const uint8_t peer_y[ZF_PUBLIC_KEY_LEN], uint8_t out[32]);
bool zf_p256_sign_hash_raw(const uint8_t private_key[ZF_PRIVATE_KEY_LEN], const uint8_t hash[32],
                           uint8_t out[ZF_PUBLIC_KEY_LEN * 2U]);
bool zf_p256_verify_hash_raw(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                             const uint8_t public_y[ZF_PUBLIC_KEY_LEN], const uint8_t hash[32],
                             const uint8_t raw_signature[ZF_PUBLIC_KEY_LEN * 2U]);
