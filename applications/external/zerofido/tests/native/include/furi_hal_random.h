#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT 11

void furi_hal_random_fill_buf(void *buffer, size_t size);
bool furi_hal_crypto_enclave_ensure_key(uint8_t slot);
bool furi_hal_crypto_enclave_load_key(uint8_t slot, const uint8_t *iv);
void furi_hal_crypto_enclave_unload_key(uint8_t slot);
bool furi_hal_crypto_load_key(const uint8_t *key, const uint8_t *iv);
bool furi_hal_crypto_unload_key(void);
bool furi_hal_crypto_encrypt(const uint8_t *input, uint8_t *output, size_t size);
bool furi_hal_crypto_decrypt(const uint8_t *input, uint8_t *output, size_t size);
