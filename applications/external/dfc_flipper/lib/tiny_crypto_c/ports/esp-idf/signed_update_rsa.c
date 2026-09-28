/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "esp_secure_boot.h"
#include "secure_boot_signature_priv.h"
#include <tiny_crypto/rsa.h>
#include <stdlib.h>

/* ESP secure-boot v2 stores RSA-3072 fields in little-endian byte order. */
static void tc_image_reverse(uint8_t *output, const uint8_t *input, size_t length)
{
    for (size_t i = 0; i < length; ++i) output[i] = input[length - i - 1];
}

esp_err_t verify_rsa_signature_block(const ets_secure_boot_signature_t *signatures,
    const uint8_t *digest, const ets_secure_boot_sig_block_t *trusted)
{
    struct verification {
        uint8_t modulus[384], signature[384], exponent[4];
        TC_RSA_word words[9 * (3072 / TC_RSA_WORD_BITS) + 2];
    } *state;
    TC_RSA_result result;
    size_t first = 0;
    uint32_t exponent;
    if (!signatures || !digest || !trusted) return ESP_ERR_INVALID_ARG;
    state = malloc(sizeof *state);
    if (!state) return ESP_ERR_NO_MEM;
    tc_image_reverse(state->modulus, (const uint8_t *)trusted->key.n, sizeof state->modulus);
    tc_image_reverse(state->signature, trusted->signature, sizeof state->signature);
    exponent = trusted->key.e;
    for (size_t i = 0; i < 4; ++i) state->exponent[3 - i] = (uint8_t)(exponent >> (8 * i));
    while (first < 3 && state->exponent[first] == 0) ++first;
    TC_RSA_public_key key = {{state->modulus,sizeof state->modulus},
                            {state->exponent + first,4 - first}};
    TC_RSA_workspace workspace = {state->words,sizeof state->words / sizeof state->words[0]};
    /* Espressif's signing format fixes SHA-256, MGF1-SHA256 and a 32-byte salt. */
    const TC_RSA_pss_options options = {TC_HASH_SHA256,TC_HASH_SHA256,32};
    TC_work_budget work = {32768};
    result = TC_RSA_verify_pss_digest(&key,&options,(TC_bytes){digest,32},
        (TC_bytes){state->signature,sizeof state->signature},&workspace,&work);
    TC_secure_zero(state,sizeof *state);
    free(state);
    return result == TC_RSA_OK ? ESP_OK : ESP_ERR_IMAGE_INVALID;
}
