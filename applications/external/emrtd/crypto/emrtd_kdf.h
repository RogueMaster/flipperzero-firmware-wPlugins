/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Key derivation, ICAO Doc 9303 part 11 section 9.7.
 *
 *     KDF(K, c) = H(K || c)
 *
 * with a 32 bit big-endian counter. SHA-1 feeds 3DES and AES-128, SHA-256
 * feeds AES-192 and AES-256. For 3DES the DES parity bits are adjusted.
 */
#pragma once

#include "emrtd_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMRTD_KDF_COUNTER_ENC  1u /**< Encryption key (9.7.2). */
#define EMRTD_KDF_COUNTER_MAC  2u /**< MAC key (9.7.2). */
#define EMRTD_KDF_COUNTER_PACE 3u /**< Key derived from the PACE password (9.7.3). */

/**
 * Derive one key of emrtd_cipher_key_size(@p cipher) bytes.
 *
 * @param[out] out_key buffer of at least EMRTD_KEY_MAX_SIZE bytes
 */
bool emrtd_kdf(
    EmrtdCipher cipher,
    const uint8_t* secret,
    size_t secret_len,
    uint32_t counter,
    uint8_t* out_key);

/** Derive the pair (K_Enc, K_MAC), counters 1 and 2. */
bool emrtd_kdf_enc_mac(
    EmrtdCipher cipher,
    const uint8_t* secret,
    size_t secret_len,
    uint8_t* out_enc,
    uint8_t* out_mac);

/** Set odd parity on every byte of a DES key. */
void emrtd_des_adjust_parity(uint8_t* key, size_t len);

#ifdef __cplusplus
}
#endif
