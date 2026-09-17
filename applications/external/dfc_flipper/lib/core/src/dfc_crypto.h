#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Crypto adapter for the small block-cipher surface that DFC uses.
 *
 * The functions accept separate input and output buffers. The buffers can also
 * be the same buffer. CBC functions update the IV to the final chaining value.
 */
bool dfc_crypto_aes_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    const uint8_t* input,
    uint8_t* output,
    size_t length);

bool dfc_crypto_des_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length);

bool dfc_crypto_des_ecb(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]);
