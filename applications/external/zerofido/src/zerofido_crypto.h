/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "crypto/hmac_sha256.h"
#include "crypto/p256.h"
#include "zerofido_types.h"

/*
 * Crypto helpers wrap the small subset of primitives used by CTAP, U2F, store,
 * and PIN. They return false on invalid inputs or backend failures and wipe
 * transient secret buffers before returning.
 */
bool zf_crypto_ensure_store_key(void);
void zf_crypto_secure_zero(void *data, size_t size);
void zf_crypto_sha256(const uint8_t *data, size_t size, uint8_t out[32]);
void zf_crypto_sha256_concat(const uint8_t *first, size_t first_size, const uint8_t *second,
                             size_t second_size, uint8_t out[32]);
bool zf_crypto_hmac_sha256_parts_with_scratch(ZfHmacSha256Scratch *scratch, const uint8_t *key,
                                              size_t key_len, const uint8_t *first,
                                              size_t first_size, const uint8_t *second,
                                              size_t second_size, uint8_t out[32]);
bool zf_crypto_hmac_sha256_parts(const uint8_t *key, size_t key_len, const uint8_t *first,
                                 size_t first_size, const uint8_t *second, size_t second_size,
                                 uint8_t out[32]);
bool zf_crypto_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t size,
                           uint8_t out[32]);
bool zf_crypto_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                           const uint8_t *info, size_t info_len, uint8_t out[32]);
/*
 * Raw AES-256-CBC helpers. Input length must be a non-zero multiple of 16;
 * callers own padding/format validation. Zero-IV variants exist only for CTAP
 * PIN protocol v1 compatibility.
 */
bool zf_crypto_aes256_cbc_encrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                                  uint8_t *output, size_t size);
bool zf_crypto_aes256_cbc_decrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                                  uint8_t *output, size_t size);
bool zf_crypto_aes256_cbc_zero_iv_encrypt(const uint8_t key[32], const uint8_t *input,
                                          uint8_t *output, size_t size);
bool zf_crypto_aes256_cbc_zero_iv_decrypt(const uint8_t key[32], const uint8_t *input,
                                          uint8_t *output, size_t size);
bool zf_crypto_generate_key_agreement_key(ZfP256KeyAgreementKey *key);
bool zf_crypto_p256_private_key_valid(const uint8_t private_key[ZF_PRIVATE_KEY_LEN]);
bool zf_crypto_p256_public_key_valid(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                     const uint8_t public_y[ZF_PUBLIC_KEY_LEN]);
/*
 * Computes P-256 ECDH and returns the X coordinate only after validating the
 * peer point is on-curve. Protocol callers decide whether to hash or HKDF this
 * raw secret.
 */
bool zf_crypto_ecdh_raw_secret(const ZfP256KeyAgreementKey *key,
                               const uint8_t peer_x[ZF_PUBLIC_KEY_LEN],
                               const uint8_t peer_y[ZF_PUBLIC_KEY_LEN], uint8_t out[32]);
bool zf_crypto_ecdh_shared_secret(const ZfP256KeyAgreementKey *key,
                                  const uint8_t peer_x[ZF_PUBLIC_KEY_LEN],
                                  const uint8_t peer_y[ZF_PUBLIC_KEY_LEN], uint8_t out[32]);
bool zf_crypto_generate_credential_keypair(ZfCredentialRecord *record);
bool zf_crypto_compute_public_key_from_private(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                               uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                               uint8_t public_y[ZF_PUBLIC_KEY_LEN]);
/*
 * Signature helpers use DER-encoded ECDSA signatures because CTAP and U2F
 * response encoders both publish ASN.1 DER, not raw r||s values.
 */
bool zf_crypto_sign_hash_with_private_key(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                          const uint8_t hash[32], uint8_t *out, size_t out_capacity,
                                          size_t *out_len);
bool zf_crypto_sign_hash_raw(const uint8_t private_key[ZF_PRIVATE_KEY_LEN], const uint8_t hash[32],
                             uint8_t out[ZF_PUBLIC_KEY_LEN * 2U]);
bool zf_crypto_verify_hash_with_public_key(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                           const uint8_t public_y[ZF_PUBLIC_KEY_LEN],
                                           const uint8_t hash[32], const uint8_t *signature,
                                           size_t signature_len);
bool zf_crypto_sign_hash(const ZfCredentialRecord *record, const uint8_t hash[32], uint8_t *out,
                         size_t out_capacity, size_t *out_len);
bool zf_crypto_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t size);
