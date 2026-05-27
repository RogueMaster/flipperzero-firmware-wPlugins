#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool furi_hal_crypto_load_key(const uint8_t *key, const uint8_t *iv);
bool furi_hal_crypto_unload_key(void);
bool furi_hal_crypto_encrypt(const uint8_t *input, uint8_t *output, size_t size);
bool furi_hal_crypto_decrypt(const uint8_t *input, uint8_t *output, size_t size);
