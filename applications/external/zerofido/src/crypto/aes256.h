/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool zf_aes256_cbc_encrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                           uint8_t *output, size_t size);
bool zf_aes256_cbc_decrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                           uint8_t *output, size_t size);
bool zf_aes256_cbc_zero_iv_encrypt(const uint8_t key[32], const uint8_t *input, uint8_t *output,
                                   size_t size);
bool zf_aes256_cbc_zero_iv_decrypt(const uint8_t key[32], const uint8_t *input, uint8_t *output,
                                   size_t size);
