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

#include "protocol.h"

#include <furi_hal_random.h>
#include <string.h>

#define ZF_PIN_PROTOCOL2_HMAC_INFO "CTAP2 HMAC key"
#define ZF_PIN_PROTOCOL2_AES_INFO "CTAP2 AES key"
#define ZF_PIN_PROTOCOL_KEY_LEN 32U

uint8_t *zf_pin_protocol_hmac_key(uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN]) {
    return keys;
}

uint8_t *zf_pin_protocol_aes_key(uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN]) {
    return keys + ZF_PIN_PROTOCOL_KEY_LEN;
}

bool zf_pin_protocol_supported(uint64_t pin_protocol) {
    return pin_protocol == ZF_PIN_PROTOCOL_V1 || pin_protocol == ZF_PIN_PROTOCOL_V2;
}

size_t zf_pin_protocol_auth_len(uint64_t pin_protocol) {
    return pin_protocol == ZF_PIN_PROTOCOL_V2 ? ZF_PIN_AUTH_MAX_LEN : ZF_PIN_AUTH_LEN;
}

/*
 * CTAP PIN protocol key schedule:
 * v1: sharedSecret = SHA-256(ECDH X), used as both AES and HMAC key.
 * v2: raw ECDH X is expanded with HKDF-SHA256 into separate HMAC/AES keys
 * using the CTAP2 labels above. Callers must zero keys after use.
 */
bool zf_pin_protocol_derive_keys(const ZfClientPinState *state, uint64_t pin_protocol,
                                 const uint8_t platform_x[ZF_PUBLIC_KEY_LEN],
                                 const uint8_t platform_y[ZF_PUBLIC_KEY_LEN],
                                 uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN]) {
    const uint8_t hkdf_salt[32] = {0};
    uint8_t raw_secret[32] = {0};
    bool ok = false;

    zf_crypto_secure_zero(keys, ZF_PIN_PROTOCOL_KEYS_LEN);
    if (!state || !platform_x || !platform_y) {
        return false;
    }
    if (pin_protocol == ZF_PIN_PROTOCOL_V1) {
        if (!zf_crypto_ecdh_shared_secret(&state->key_agreement, platform_x, platform_y,
                                          zf_pin_protocol_hmac_key(keys))) {
            return false;
        }
        memcpy(zf_pin_protocol_aes_key(keys), zf_pin_protocol_hmac_key(keys),
               ZF_PIN_PROTOCOL_KEY_LEN);
        return true;
    }
    if (pin_protocol != ZF_PIN_PROTOCOL_V2) {
        return false;
    }

    do {
        if (!zf_crypto_ecdh_raw_secret(&state->key_agreement, platform_x, platform_y, raw_secret)) {
            break;
        }
        if (!zf_crypto_hkdf_sha256(hkdf_salt, sizeof(hkdf_salt), raw_secret, sizeof(raw_secret),
                                   (const uint8_t *)ZF_PIN_PROTOCOL2_HMAC_INFO,
                                   sizeof(ZF_PIN_PROTOCOL2_HMAC_INFO) - 1U,
                                   zf_pin_protocol_hmac_key(keys))) {
            break;
        }
        if (!zf_crypto_hkdf_sha256(hkdf_salt, sizeof(hkdf_salt), raw_secret, sizeof(raw_secret),
                                   (const uint8_t *)ZF_PIN_PROTOCOL2_AES_INFO,
                                   sizeof(ZF_PIN_PROTOCOL2_AES_INFO) - 1U,
                                   zf_pin_protocol_aes_key(keys))) {
            break;
        }
        ok = true;
    } while (false);

    zf_crypto_secure_zero(raw_secret, sizeof(raw_secret));
    if (!ok) {
        zf_crypto_secure_zero(keys, ZF_PIN_PROTOCOL_KEYS_LEN);
    }
    return ok;
}

bool zf_pin_protocol_decrypt(uint64_t pin_protocol, uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN],
                             const uint8_t *ciphertext, size_t ciphertext_len, uint8_t *plaintext,
                             size_t *plaintext_len) {
    if (!ciphertext || !plaintext || !plaintext_len) {
        return false;
    }
    if (pin_protocol == ZF_PIN_PROTOCOL_V1) {
        if (!zf_crypto_aes256_cbc_zero_iv_decrypt(zf_pin_protocol_aes_key(keys), ciphertext,
                                                  plaintext, ciphertext_len)) {
            return false;
        }
        *plaintext_len = ciphertext_len;
        return true;
    }
    if (pin_protocol != ZF_PIN_PROTOCOL_V2 || ciphertext_len <= ZF_PIN_PROTOCOL2_IV_LEN ||
        ((ciphertext_len - ZF_PIN_PROTOCOL2_IV_LEN) % 16U) != 0U) {
        return false;
    }

    if (!zf_crypto_aes256_cbc_decrypt(zf_pin_protocol_aes_key(keys), ciphertext,
                                      ciphertext + ZF_PIN_PROTOCOL2_IV_LEN, plaintext,
                                      ciphertext_len - ZF_PIN_PROTOCOL2_IV_LEN)) {
        return false;
    }
    *plaintext_len = ciphertext_len - ZF_PIN_PROTOCOL2_IV_LEN;
    return true;
}

/*
 * Encrypts protocol v1 payloads with the implicit zero IV and protocol v2
 * payloads with a random IV prepended to the ciphertext.
 */
bool zf_pin_protocol_encrypt(uint64_t pin_protocol, uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN],
                             const uint8_t *plaintext, size_t plaintext_len, uint8_t *out,
                             size_t out_capacity, size_t *out_len) {
    if (!plaintext || !out || !out_len || plaintext_len == 0U || (plaintext_len % 16U) != 0U) {
        return false;
    }
    if (pin_protocol == ZF_PIN_PROTOCOL_V1) {
        if (out_capacity < plaintext_len ||
            !zf_crypto_aes256_cbc_zero_iv_encrypt(zf_pin_protocol_aes_key(keys), plaintext, out,
                                                  plaintext_len)) {
            return false;
        }
        *out_len = plaintext_len;
        return true;
    }
    if (pin_protocol != ZF_PIN_PROTOCOL_V2 ||
        out_capacity < ZF_PIN_PROTOCOL2_IV_LEN + plaintext_len) {
        return false;
    }

    furi_hal_random_fill_buf(out, ZF_PIN_PROTOCOL2_IV_LEN);
    if (!zf_crypto_aes256_cbc_encrypt(zf_pin_protocol_aes_key(keys), out, plaintext,
                                      out + ZF_PIN_PROTOCOL2_IV_LEN, plaintext_len)) {
        return false;
    }
    *out_len = ZF_PIN_PROTOCOL2_IV_LEN + plaintext_len;
    return true;
}

/* Computes the full HMAC but compares only the protocol-defined auth prefix. */
bool zf_pin_protocol_hmac_matches(ZfHmacSha256Scratch *scratch, uint64_t pin_protocol,
                                  const uint8_t key[32], const uint8_t *first, size_t first_len,
                                  const uint8_t *second, size_t second_len, const uint8_t *expected,
                                  size_t expected_len) {
    uint8_t hmac[32];
    bool matches = false;

    if (!zf_crypto_hmac_sha256_parts_with_scratch(scratch, key, 32, first, first_len, second,
                                                  second_len, hmac)) {
        return false;
    }
    size_t auth_len = zf_pin_protocol_auth_len(pin_protocol);
    matches = expected_len == auth_len && zf_crypto_constant_time_equal(hmac, expected, auth_len);
    zf_crypto_secure_zero(hmac, sizeof(hmac));
    return matches;
}
