/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "aes256.h"

#include <furi_hal_crypto.h>
#include <string.h>

#define ZF_AES_BLOCK_LEN 16U
#define ZF_AES_KEY_LEN 32U
#define ZF_AES_MAX_CBC_LEN 128U

static void zf_aes_secure_zero(void *data, size_t size) {
    volatile uint8_t *ptr = data;

    if (!ptr) {
        return;
    }
    while (size-- > 0U) {
        *ptr++ = 0;
    }
}

static void zf_aes_bswap_words(uint8_t *out, const uint8_t *in, size_t size) {
    for (size_t offset = 0; offset < size; offset += 4U) {
        out[offset + 0U] = in[offset + 3U];
        out[offset + 1U] = in[offset + 2U];
        out[offset + 2U] = in[offset + 1U];
        out[offset + 3U] = in[offset + 0U];
    }
}

static bool zf_aes256_cbc_crypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                                uint8_t *output, size_t size, bool decrypt) {
    uint32_t hal_key_words[ZF_AES_KEY_LEN / sizeof(uint32_t)];
    uint32_t hal_iv_words[ZF_AES_BLOCK_LEN / sizeof(uint32_t)];
    uint32_t hal_input_words[ZF_AES_MAX_CBC_LEN / sizeof(uint32_t)];
    uint32_t hal_output_words[ZF_AES_MAX_CBC_LEN / sizeof(uint32_t)];
    uint8_t *hal_key = (uint8_t *)hal_key_words;
    uint8_t *hal_iv = (uint8_t *)hal_iv_words;
    uint8_t *hal_input = (uint8_t *)hal_input_words;
    uint8_t *hal_output = (uint8_t *)hal_output_words;
    bool loaded = false;
    bool ok = false;

    if (!key || !iv || !input || !output || size == 0U || size > sizeof(hal_input_words) ||
        (size % ZF_AES_BLOCK_LEN) != 0U) {
        return false;
    }

    zf_aes_bswap_words(hal_key, key, ZF_AES_KEY_LEN);
    zf_aes_bswap_words(hal_iv, iv, ZF_AES_BLOCK_LEN);
    zf_aes_bswap_words(hal_input, input, size);
    memset(hal_output_words, 0, sizeof(hal_output_words));

    loaded = furi_hal_crypto_load_key(hal_key, hal_iv);
    if (loaded) {
        ok = decrypt ? furi_hal_crypto_decrypt(hal_input, hal_output, size) :
                       furi_hal_crypto_encrypt(hal_input, hal_output, size);
        (void)furi_hal_crypto_unload_key();
    }
    if (ok) {
        zf_aes_bswap_words(output, hal_output, size);
    }

    zf_aes_secure_zero(hal_key_words, sizeof(hal_key_words));
    zf_aes_secure_zero(hal_iv_words, sizeof(hal_iv_words));
    zf_aes_secure_zero(hal_input_words, sizeof(hal_input_words));
    zf_aes_secure_zero(hal_output_words, sizeof(hal_output_words));
    return loaded && ok;
}

bool zf_aes256_cbc_encrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                           uint8_t *output, size_t size) {
    return zf_aes256_cbc_crypt(key, iv, input, output, size, false);
}

bool zf_aes256_cbc_decrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                           uint8_t *output, size_t size) {
    return zf_aes256_cbc_crypt(key, iv, input, output, size, true);
}

bool zf_aes256_cbc_zero_iv_encrypt(const uint8_t key[32], const uint8_t *input, uint8_t *output,
                                   size_t size) {
    const uint8_t iv[ZF_AES_BLOCK_LEN] = {0};
    return zf_aes256_cbc_encrypt(key, iv, input, output, size);
}

bool zf_aes256_cbc_zero_iv_decrypt(const uint8_t key[32], const uint8_t *input, uint8_t *output,
                                   size_t size) {
    const uint8_t iv[ZF_AES_BLOCK_LEN] = {0};
    return zf_aes256_cbc_decrypt(key, iv, input, output, size);
}
