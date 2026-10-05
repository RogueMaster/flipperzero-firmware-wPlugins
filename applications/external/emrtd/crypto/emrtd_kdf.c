/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_kdf.h"

#include <string.h>

#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

#include "../emrtd_wipe.h"

void emrtd_des_adjust_parity(uint8_t* key, size_t len) {
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = key[i] & 0xFE;
        uint8_t ones = 0;
        for(uint8_t bit = 1; bit < 8; bit++) {
            if(byte & (1u << bit)) {
                ones++;
            }
        }
        /* Complete every byte to an odd number of one bits. */
        key[i] = (uint8_t)(byte | ((ones % 2 == 0) ? 0x01 : 0x00));
    }
}

bool emrtd_kdf(
    EmrtdCipher cipher,
    const uint8_t* secret,
    size_t secret_len,
    uint32_t counter,
    uint8_t* out_key) {
    const size_t key_len = emrtd_cipher_key_size(cipher);
    if(key_len == 0) {
        return false;
    }

    const uint8_t counter_be[4] = {
        (uint8_t)(counter >> 24),
        (uint8_t)(counter >> 16),
        (uint8_t)(counter >> 8),
        (uint8_t)counter,
    };

    uint8_t digest[32];
    if(cipher == EmrtdCipherTdes || cipher == EmrtdCipherAes128) {
        mbedtls_sha1_context ctx;
        mbedtls_sha1_init(&ctx);
        mbedtls_sha1_starts(&ctx);
        mbedtls_sha1_update(&ctx, secret, secret_len);
        mbedtls_sha1_update(&ctx, counter_be, sizeof(counter_be));
        mbedtls_sha1_finish(&ctx, digest);
        mbedtls_sha1_free(&ctx);
    } else {
        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts(&ctx, 0);
        mbedtls_sha256_update(&ctx, secret, secret_len);
        mbedtls_sha256_update(&ctx, counter_be, sizeof(counter_be));
        mbedtls_sha256_finish(&ctx, digest);
        mbedtls_sha256_free(&ctx);
    }

    memcpy(out_key, digest, key_len);
    if(cipher == EmrtdCipherTdes) {
        emrtd_des_adjust_parity(out_key, 16);
    }

    emrtd_secure_wipe(digest, sizeof(digest));
    return true;
}

bool emrtd_kdf_enc_mac(
    EmrtdCipher cipher,
    const uint8_t* secret,
    size_t secret_len,
    uint8_t* out_enc,
    uint8_t* out_mac) {
    return emrtd_kdf(cipher, secret, secret_len, EMRTD_KDF_COUNTER_ENC, out_enc) &&
           emrtd_kdf(cipher, secret, secret_len, EMRTD_KDF_COUNTER_MAC, out_mac);
}
