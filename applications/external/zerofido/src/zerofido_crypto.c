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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "zerofido_crypto.h"

#include <furi_hal.h>
#include <furi_hal_random.h>
#include <string.h>

#include "crypto/aes256.h"
#include "crypto/ecdsa_der.h"
#include "crypto/hmac_sha256.h"
#include "crypto/p256.h"

#define ZF_UNIQUE_KEY_SLOT FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT

/* Unwraps a credential private key with the device unique key and caller-owned IV. */
static bool zf_unwrap_private_key(const ZfCredentialRecord *record,
                                  uint8_t private_key[ZF_PRIVATE_KEY_LEN]) {
    if (!record || !private_key ||
        !furi_hal_crypto_enclave_load_key(ZF_UNIQUE_KEY_SLOT, record->private_iv)) {
        return false;
    }

    bool ok = furi_hal_crypto_decrypt(record->private_wrapped, private_key,
                                      sizeof(record->private_wrapped));
    furi_hal_crypto_enclave_unload_key(ZF_UNIQUE_KEY_SLOT);
    return ok;
}

/* Volatile-store writes are used for deliberate secret erasure. */
void zf_crypto_secure_zero(void *data, size_t size) {
    volatile uint8_t *ptr = data;

    if (!ptr) {
        return;
    }

    while (size-- > 0U) {
        *ptr++ = 0;
    }
}

bool zf_crypto_ensure_store_key(void) {
    return furi_hal_crypto_enclave_ensure_key(ZF_UNIQUE_KEY_SLOT);
}

void zf_crypto_sha256(const uint8_t *data, size_t size, uint8_t out[32]) {
    ZfSha256Context sha;

    zf_sha256_init(&sha);
    zf_sha256_update(&sha, data, size);
    zf_sha256_finish(&sha, out);
}

void zf_crypto_sha256_concat(const uint8_t *first, size_t first_size, const uint8_t *second,
                             size_t second_size, uint8_t out[32]) {
    ZfSha256Context sha;

    zf_sha256_init(&sha);
    if (first_size > 0U) {
        zf_sha256_update(&sha, first, first_size);
    }
    if (second_size > 0U) {
        zf_sha256_update(&sha, second, second_size);
    }
    zf_sha256_finish(&sha, out);
}

bool zf_crypto_hmac_sha256_parts_with_scratch(ZfHmacSha256Scratch *scratch, const uint8_t *key,
                                              size_t key_len, const uint8_t *first,
                                              size_t first_size, const uint8_t *second,
                                              size_t second_size, uint8_t out[32]) {
    return zf_hmac_sha256_parts_with_scratch(scratch, key, key_len, first, first_size, second,
                                             second_size, out);
}

bool zf_crypto_hmac_sha256_parts(const uint8_t *key, size_t key_len, const uint8_t *first,
                                 size_t first_size, const uint8_t *second, size_t second_size,
                                 uint8_t out[32]) {
    return zf_hmac_sha256_parts(key, key_len, first, first_size, second, second_size, out);
}

bool zf_crypto_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t size,
                           uint8_t out[32]) {
    return zf_hmac_sha256(key, key_len, data, size, out);
}

bool zf_crypto_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                           const uint8_t *info, size_t info_len, uint8_t out[32]) {
    return zf_hkdf_sha256(salt, salt_len, ikm, ikm_len, info, info_len, out);
}

bool zf_crypto_aes256_cbc_encrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                                  uint8_t *output, size_t size) {
    return zf_aes256_cbc_encrypt(key, iv, input, output, size);
}

bool zf_crypto_aes256_cbc_decrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                                  uint8_t *output, size_t size) {
    return zf_aes256_cbc_decrypt(key, iv, input, output, size);
}

bool zf_crypto_aes256_cbc_zero_iv_encrypt(const uint8_t key[32], const uint8_t *input,
                                          uint8_t *output, size_t size) {
    return zf_aes256_cbc_zero_iv_encrypt(key, input, output, size);
}

bool zf_crypto_aes256_cbc_zero_iv_decrypt(const uint8_t key[32], const uint8_t *input,
                                          uint8_t *output, size_t size) {
    return zf_aes256_cbc_zero_iv_decrypt(key, input, output, size);
}

bool zf_crypto_generate_key_agreement_key(ZfP256KeyAgreementKey *key) {
    bool ok = false;
    ZfP256KeyAgreementKey generated = {0};

    if (!key) {
        return false;
    }

    if (zf_p256_generate_keypair(generated.private_key, generated.public_x, generated.public_y)) {
        *key = generated;
        ok = true;
    }
    if (!ok) {
        zf_crypto_secure_zero(key, sizeof(*key));
    }
    zf_crypto_secure_zero(&generated, sizeof(generated));
    return ok;
}

bool zf_crypto_p256_private_key_valid(const uint8_t private_key[ZF_PRIVATE_KEY_LEN]) {
    return zf_p256_private_key_valid(private_key);
}

bool zf_crypto_p256_public_key_valid(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                     const uint8_t public_y[ZF_PUBLIC_KEY_LEN]) {
    return zf_p256_public_key_valid(public_x, public_y);
}

bool zf_crypto_ecdh_raw_secret(const ZfP256KeyAgreementKey *key,
                               const uint8_t peer_x[ZF_PUBLIC_KEY_LEN],
                               const uint8_t peer_y[ZF_PUBLIC_KEY_LEN], uint8_t out[32]) {
    return zf_p256_ecdh_raw_secret(key, peer_x, peer_y, out);
}

bool zf_crypto_ecdh_shared_secret(const ZfP256KeyAgreementKey *key,
                                  const uint8_t peer_x[ZF_PUBLIC_KEY_LEN],
                                  const uint8_t peer_y[ZF_PUBLIC_KEY_LEN], uint8_t out[32]) {
    uint8_t secret_x[32] = {0};

    if (!zf_p256_ecdh_raw_secret(key, peer_x, peer_y, secret_x)) {
        zf_crypto_secure_zero(secret_x, sizeof(secret_x));
        return false;
    }
    zf_crypto_sha256(secret_x, sizeof(secret_x), out);
    zf_crypto_secure_zero(secret_x, sizeof(secret_x));
    return true;
}

/* Generates a credential keypair and wraps only the private scalar for storage. */
bool zf_crypto_generate_credential_keypair(ZfCredentialRecord *record) {
    bool ok = false;
    uint8_t iv[ZF_WRAP_IV_LEN] = {0};
    uint8_t private_key[ZF_PRIVATE_KEY_LEN] = {0};

    do {
        if (!record || !zf_p256_generate_keypair(private_key, record->public_x, record->public_y)) {
            break;
        }

        furi_hal_random_fill_buf(iv, sizeof(iv));
        memcpy(record->private_iv, iv, sizeof(iv));

        if (!furi_hal_crypto_enclave_load_key(ZF_UNIQUE_KEY_SLOT, iv)) {
            break;
        }
        if (!furi_hal_crypto_encrypt(private_key, record->private_wrapped,
                                     sizeof(record->private_wrapped))) {
            furi_hal_crypto_enclave_unload_key(ZF_UNIQUE_KEY_SLOT);
            break;
        }
        furi_hal_crypto_enclave_unload_key(ZF_UNIQUE_KEY_SLOT);
        ok = true;
    } while (false);

    zf_crypto_secure_zero(private_key, sizeof(private_key));
    return ok;
}

bool zf_crypto_compute_public_key_from_private(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                               uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                               uint8_t public_y[ZF_PUBLIC_KEY_LEN]) {
    return zf_p256_compute_public_key(private_key, public_x, public_y);
}

bool zf_crypto_sign_hash_raw(const uint8_t private_key[ZF_PRIVATE_KEY_LEN], const uint8_t hash[32],
                             uint8_t out[ZF_PUBLIC_KEY_LEN * 2U]) {
    return zf_p256_sign_hash_raw(private_key, hash, out);
}

/*
 * Signs an already-computed SHA-256 digest with a raw P-256 private key and
 * emits DER. The caller owns hash construction so CTAP and attestation can bind
 * different transcript formats.
 */
bool zf_crypto_sign_hash_with_private_key(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                          const uint8_t hash[32], uint8_t *out, size_t out_capacity,
                                          size_t *out_len) {
    uint8_t signature[ZF_PUBLIC_KEY_LEN * 2U];
    bool ok = false;

    if (!out_len || !zf_p256_sign_hash_raw(private_key, hash, signature)) {
        goto cleanup;
    }
    *out_len =
        zf_ecdsa_der_encode_signature(signature, signature + ZF_PUBLIC_KEY_LEN, out, out_capacity);
    ok = *out_len > 0U;

cleanup:
    zf_crypto_secure_zero(signature, sizeof(signature));
    return ok;
}

/* Validates a DER ECDSA signature against an explicit P-256 public point. */
bool zf_crypto_verify_hash_with_public_key(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                           const uint8_t public_y[ZF_PUBLIC_KEY_LEN],
                                           const uint8_t hash[32], const uint8_t *signature,
                                           size_t signature_len) {
    uint8_t raw_signature[ZF_PUBLIC_KEY_LEN * 2U];
    bool ok = false;

    if (!zf_ecdsa_der_decode_signature(signature, signature_len, raw_signature)) {
        goto cleanup;
    }
    ok = zf_p256_verify_hash_raw(public_x, public_y, hash, raw_signature);

cleanup:
    zf_crypto_secure_zero(raw_signature, sizeof(raw_signature));
    return ok;
}

/* Signs with a stored credential after decrypting its wrapped private scalar. */
bool zf_crypto_sign_hash(const ZfCredentialRecord *record, const uint8_t hash[32], uint8_t *out,
                         size_t out_capacity, size_t *out_len) {
    bool ok = false;
    uint8_t private_key[ZF_PRIVATE_KEY_LEN] = {0};

    do {
        if (!zf_unwrap_private_key(record, private_key)) {
            break;
        }
        ok = zf_crypto_sign_hash_with_private_key(private_key, hash, out, out_capacity, out_len);
    } while (false);

    zf_crypto_secure_zero(private_key, sizeof(private_key));
    return ok;
}

/* Fixed-work equality check for secrets, MACs, and hashed RP IDs. */
bool zf_crypto_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t size) {
    uint8_t diff = 0;

    if (!left || !right) {
        return false;
    }

    for (size_t i = 0; i < size; ++i) {
        diff |= left[i] ^ right[i];
    }

    return diff == 0;
}
