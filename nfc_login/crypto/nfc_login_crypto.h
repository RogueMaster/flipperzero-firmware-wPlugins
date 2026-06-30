#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AES_BLOCK_SIZE 16
#define MAX_ENCRYPTED_SIZE 4096
#define AES_KEY_SIZE 32  // AES-256

bool ensure_crypto_key(void);
bool encrypt_data(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len);
bool decrypt_data(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len);

// Passcode-based encryption functions (using button sequence)
bool encrypt_data_with_passcode_sequence(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len, const char* sequence);
bool decrypt_data_with_passcode_sequence(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len, const char* sequence);