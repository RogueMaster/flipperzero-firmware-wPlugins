#include "nfc_login_crypto.h"
#include "nfc_login_passcode.h"

#include <furi.h>
#include <furi_hal_crypto.h>
#include <furi_hal_version.h>
#include <stdlib.h>
#include <string.h>

#define TAG             "nfc_login_crypto"
#define CRYPTO_KEY_SLOT FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT

static void generate_iv_from_device_uid(uint8_t* iv) {
    const uint8_t* device_uid = furi_hal_version_uid();
    size_t uid_size = furi_hal_version_uid_size();

    memset(iv, 0, AES_BLOCK_SIZE);
    if(uid_size > 0) {
        size_t copy_size = uid_size < AES_BLOCK_SIZE ? uid_size : AES_BLOCK_SIZE;
        memcpy(iv, device_uid, copy_size);
        for(size_t i = copy_size; i < AES_BLOCK_SIZE; i++) {
            iv[i] = device_uid[i % uid_size];
        }
    }
}

bool ensure_crypto_key(void) {
    if(furi_hal_crypto_enclave_ensure_key(CRYPTO_KEY_SLOT)) {
        return true;
    }
    FURI_LOG_E(TAG, "ensure_crypto_key: Failed to ensure crypto key slot %d", CRYPTO_KEY_SLOT);
    return false;
}

bool encrypt_data(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len) {
    if(!input || !output || !output_len || input_len == 0) {
        FURI_LOG_E(TAG, "encrypt_data: Invalid parameters");
        return false;
    }

    if(input_len > MAX_ENCRYPTED_SIZE) {
        FURI_LOG_E(TAG, "encrypt_data: Input too large: %zu", input_len);
        return false;
    }

    uint8_t iv[AES_BLOCK_SIZE];
    generate_iv_from_device_uid(iv);

    size_t padded_len = ((input_len + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
    if(padded_len > MAX_ENCRYPTED_SIZE) {
        FURI_LOG_E(TAG, "encrypt_data: Padded length too large: %zu", padded_len);
        return false;
    }

    uint8_t* padded_input = malloc(padded_len);
    if(!padded_input) {
        FURI_LOG_E(TAG, "encrypt_data: Failed to allocate padded buffer");
        return false;
    }

    memcpy(padded_input, input, input_len);
    uint8_t pad_value = padded_len - input_len;
    memset(padded_input + input_len, pad_value, pad_value);

    bool success = false;
    if(furi_hal_crypto_enclave_load_key(CRYPTO_KEY_SLOT, iv)) {
        success = furi_hal_crypto_encrypt(padded_input, output, padded_len);
        furi_hal_crypto_enclave_unload_key(CRYPTO_KEY_SLOT);

        if(success) {
            *output_len = padded_len;
        } else {
            FURI_LOG_E(TAG, "encrypt_data: Encryption failed");
        }
    } else {
        FURI_LOG_E(TAG, "encrypt_data: Failed to load key from enclave");
    }

    memset(padded_input, 0, padded_len);
    free(padded_input);

    return success;
}

bool decrypt_data(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len) {
    if(!input || !output || !output_len) {
        FURI_LOG_E(TAG, "decrypt_data: Invalid parameters");
        return false;
    }

    if(input_len == 0 || input_len % AES_BLOCK_SIZE != 0) {
        FURI_LOG_E(TAG, "decrypt_data: Invalid input length: %zu", input_len);
        return false;
    }

    if(input_len > MAX_ENCRYPTED_SIZE) {
        FURI_LOG_E(TAG, "decrypt_data: Input too large: %zu", input_len);
        return false;
    }

    uint8_t iv[AES_BLOCK_SIZE];
    generate_iv_from_device_uid(iv);

    bool success = false;
    if(furi_hal_crypto_enclave_load_key(CRYPTO_KEY_SLOT, iv)) {
        success = furi_hal_crypto_decrypt(input, output, input_len);
        furi_hal_crypto_enclave_unload_key(CRYPTO_KEY_SLOT);

        if(success) {
            uint8_t pad_value = output[input_len - 1];
            if(pad_value > 0 && pad_value <= AES_BLOCK_SIZE && pad_value <= input_len) {
                *output_len = input_len - pad_value;
            } else {
                *output_len = input_len;
            }
        } else {
            FURI_LOG_E(TAG, "decrypt_data: Decryption failed");
        }
    } else {
        FURI_LOG_E(TAG, "decrypt_data: Failed to load key from enclave");
    }

    return success;
}

bool encrypt_data_with_passcode_sequence(
    const uint8_t* input,
    size_t input_len,
    uint8_t* output,
    size_t* output_len,
    const char* sequence) {
    if(!input || !output || !output_len || input_len == 0 || !sequence) {
        FURI_LOG_E(TAG, "encrypt_data_with_passcode_sequence: Invalid parameters");
        return false;
    }

    size_t seq_len = strlen(sequence);
    if(seq_len == 0) {
        FURI_LOG_E(TAG, "encrypt_data_with_passcode_sequence: Invalid sequence");
        return false;
    }

    if(input_len > MAX_ENCRYPTED_SIZE) {
        FURI_LOG_E(TAG, "encrypt_data_with_passcode_sequence: Input too large: %zu", input_len);
        return false;
    }

    // Derive encryption key from button sequence
    uint8_t key[AES_KEY_SIZE];
    if(!derive_key_from_passcode_sequence(sequence, key, AES_KEY_SIZE)) {
        FURI_LOG_E(TAG, "encrypt_data_with_passcode_sequence: Failed to derive key");
        return false;
    }

    // Generate IV from device UID (same as hardware encryption for consistency)
    uint8_t iv[AES_BLOCK_SIZE];
    generate_iv_from_device_uid(iv);

    // Pad input to block size
    size_t padded_len = ((input_len + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
    if(padded_len > MAX_ENCRYPTED_SIZE) {
        FURI_LOG_E(TAG, "encrypt_data_with_passcode: Padded length too large: %zu", padded_len);
        memset(key, 0, AES_KEY_SIZE);
        return false;
    }

    uint8_t* padded_input = malloc(padded_len);
    if(!padded_input) {
        FURI_LOG_E(TAG, "encrypt_data_with_passcode: Failed to allocate padded buffer");
        memset(key, 0, AES_KEY_SIZE);
        return false;
    }

    memcpy(padded_input, input, input_len);
    uint8_t pad_value = padded_len - input_len;
    memset(padded_input + input_len, pad_value, pad_value);

    // Simple block cipher encryption (CBC mode) without mbedtls
    // Uses key mixing and XOR operations for encryption
    uint8_t iv_copy[AES_BLOCK_SIZE];
    memcpy(iv_copy, iv, AES_BLOCK_SIZE);

    for(size_t block = 0; block < padded_len; block += AES_BLOCK_SIZE) {
        uint8_t block_data[AES_BLOCK_SIZE];

        // XOR with previous ciphertext (CBC mode)
        for(size_t i = 0; i < AES_BLOCK_SIZE; i++) {
            block_data[i] = padded_input[block + i] ^ iv_copy[i];
        }

        // Simple block encryption using key
        for(int round = 0; round < 4; round++) {
            for(size_t i = 0; i < AES_BLOCK_SIZE; i++) {
                block_data[i] ^= key[(i + round) % AES_KEY_SIZE];
                block_data[i] = (block_data[i] << 1) | (block_data[i] >> 7);
                block_data[i] ^= key[(i + round + 1) % AES_KEY_SIZE];
            }
        }

        // Store ciphertext and update IV for next block
        memcpy(output + block, block_data, AES_BLOCK_SIZE);
        memcpy(iv_copy, block_data, AES_BLOCK_SIZE);
    }

    memset(padded_input, 0, padded_len);
    memset(key, 0, AES_KEY_SIZE);
    free(padded_input);

    *output_len = padded_len;
    return true;
}

bool decrypt_data_with_passcode_sequence(
    const uint8_t* input,
    size_t input_len,
    uint8_t* output,
    size_t* output_len,
    const char* sequence) {
    if(!input || !output || !output_len) {
        FURI_LOG_E(TAG, "decrypt_data_with_passcode_sequence: Invalid parameters");
        return false;
    }

    if(input_len == 0 || input_len % AES_BLOCK_SIZE != 0) {
        FURI_LOG_E(
            TAG, "decrypt_data_with_passcode_sequence: Invalid input length: %zu", input_len);
        return false;
    }

    if(input_len > MAX_ENCRYPTED_SIZE) {
        FURI_LOG_E(TAG, "decrypt_data_with_passcode_sequence: Input too large: %zu", input_len);
        return false;
    }

    if(!sequence) {
        FURI_LOG_E(TAG, "decrypt_data_with_passcode_sequence: Invalid sequence");
        return false;
    }

    if(strlen(sequence) == 0) {
        FURI_LOG_E(TAG, "decrypt_data_with_passcode_sequence: Invalid sequence");
        return false;
    }

    // Derive decryption key from button sequence
    uint8_t key[AES_KEY_SIZE];
    if(!derive_key_from_passcode_sequence(sequence, key, AES_KEY_SIZE)) {
        FURI_LOG_E(TAG, "decrypt_data_with_passcode_sequence: Failed to derive key");
        return false;
    }

    // Generate IV from device UID (same as encryption)
    uint8_t iv[AES_BLOCK_SIZE];
    generate_iv_from_device_uid(iv);

    // Simple block cipher decryption (CBC mode) without mbedtls
    uint8_t iv_copy[AES_BLOCK_SIZE];
    memcpy(iv_copy, iv, AES_BLOCK_SIZE);

    for(size_t block = 0; block < input_len; block += AES_BLOCK_SIZE) {
        uint8_t cipher_block[AES_BLOCK_SIZE];
        uint8_t block_data[AES_BLOCK_SIZE];

        memcpy(cipher_block, input + block, AES_BLOCK_SIZE);
        memcpy(block_data, cipher_block, AES_BLOCK_SIZE);

        // Simple block decryption (reverse of encryption)
        for(int round = 3; round >= 0; round--) {
            for(size_t i = 0; i < AES_BLOCK_SIZE; i++) {
                block_data[i] ^= key[(i + round + 1) % AES_KEY_SIZE];
                block_data[i] = (block_data[i] >> 1) | (block_data[i] << 7);
                block_data[i] ^= key[(i + round) % AES_KEY_SIZE];
            }
        }

        // XOR with previous ciphertext (CBC mode)
        for(size_t i = 0; i < AES_BLOCK_SIZE; i++) {
            output[block + i] = block_data[i] ^ iv_copy[i];
        }

        // Update IV for next block
        memcpy(iv_copy, cipher_block, AES_BLOCK_SIZE);
    }

    memset(key, 0, AES_KEY_SIZE);

    // Remove padding
    uint8_t pad_value = output[input_len - 1];
    if(pad_value > 0 && pad_value <= AES_BLOCK_SIZE && pad_value <= input_len) {
        *output_len = input_len - pad_value;
    } else {
        *output_len = input_len;
    }

    return true;
}
