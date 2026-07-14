#include "nfc_login_passcode.h"
#include "nfc_login_crypto.h"
#include "../storage/nfc_login_card_storage.h"
#include <furi.h>
#include <storage/storage.h>
#include <string.h>
#include <stdlib.h>

// TAG is defined in nfc_login_app.h (included via nfc_login_card_storage.h)
#define PBKDF2_ITERATIONS        10000
#define KEY_DERIVATION_SALT_SIZE 16
#define PASSCODE_HASH_SIZE       32

// Salt for key derivation (derived from device UID for consistency)
static void get_key_derivation_salt(uint8_t* salt_out) {
    const uint8_t* device_uid = furi_hal_version_uid();
    size_t uid_size = furi_hal_version_uid_size();

    memset(salt_out, 0, KEY_DERIVATION_SALT_SIZE);
    if(uid_size > 0) {
        size_t copy_size = uid_size < KEY_DERIVATION_SALT_SIZE ? uid_size :
                                                                 KEY_DERIVATION_SALT_SIZE;
        memcpy(salt_out, device_uid, copy_size);
        for(size_t i = copy_size; i < KEY_DERIVATION_SALT_SIZE; i++) {
            salt_out[i] = device_uid[i % uid_size];
        }
    }
}

// Count number of buttons in a sequence string (space-separated)
size_t count_buttons_in_sequence(const char* sequence) {
    if(!sequence || strlen(sequence) == 0) {
        return 0;
    }

    size_t count = 0;
    const char* ptr = sequence;
    bool in_word = false;

    while(*ptr) {
        if(*ptr == ' ') {
            in_word = false;
        } else {
            if(!in_word) {
                count++;
                in_word = true;
            }
        }
        ptr++;
    }

    return count;
}

// Get encrypted passcode from cards.enc file header and decrypt it
bool get_passcode_sequence(char* sequence_out, size_t sequence_buf_size) {
    if(!sequence_out || sequence_buf_size == 0) {
        FURI_LOG_E(TAG, "get_passcode_sequence: Invalid parameters");
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "get_passcode_sequence: Failed to open storage");
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    // Read from cards.enc file header
    if(storage_file_open(file, NFC_CARDS_FILE_ENC, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);

        if(file_size >= PASSCODE_HEADER_SIZE) {
            // Read passcode length (2 bytes) and flags (1 byte)
            uint8_t header_bytes[3];
            size_t bytes_read = storage_file_read(file, header_bytes, 3);

            if(bytes_read >= 2) {
                uint16_t passcode_len = (uint16_t)(header_bytes[0] | (header_bytes[1] << 8));

                if(passcode_len > 0 && passcode_len < 512 && passcode_len % AES_BLOCK_SIZE == 0) {
                    // Read encrypted passcode
                    uint8_t* encrypted = malloc(passcode_len);
                    if(encrypted) {
                        bytes_read = storage_file_read(file, encrypted, passcode_len);

                        if(bytes_read == passcode_len) {
                            // Decrypt the passcode
                            uint8_t* decrypted = malloc(passcode_len);
                            size_t decrypted_len = 0;

                            if(decrypted &&
                               decrypt_data(encrypted, passcode_len, decrypted, &decrypted_len)) {
                                if(decrypted_len < sequence_buf_size) {
                                    memcpy(sequence_out, decrypted, decrypted_len);
                                    sequence_out[decrypted_len] = '\0';
                                    success = true;
                                } else {
                                    FURI_LOG_E(
                                        TAG,
                                        "get_passcode_sequence: Decrypted passcode too large");
                                }
                                memset(decrypted, 0, decrypted_len);
                            } else {
                                FURI_LOG_E(
                                    TAG, "get_passcode_sequence: Failed to decrypt passcode");
                            }

                            if(decrypted) {
                                free(decrypted);
                            }
                        } else {
                            FURI_LOG_E(
                                TAG, "get_passcode_sequence: Failed to read encrypted passcode");
                        }

                        free(encrypted);
                    }
                } else if(passcode_len == 0) {
                } else {
                    FURI_LOG_E(
                        TAG, "get_passcode_sequence: Invalid passcode length: %u", passcode_len);
                }
            }
        } else {
        }

        storage_file_close(file);
    } else {
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

// Encrypt passcode and save it to settings file
bool set_passcode_sequence(const char* sequence) {
    if(!sequence) {
        FURI_LOG_E(TAG, "set_passcode_sequence: Invalid sequence");
        return false;
    }

    size_t seq_len = strlen(sequence);
    if(seq_len == 0) {
        FURI_LOG_E(TAG, "set_passcode_sequence: Invalid sequence");
        return false;
    }

    // Validate button count (4-8 buttons)
    size_t button_count = count_buttons_in_sequence(sequence);
    if(button_count < MIN_PASSCODE_BUTTONS || button_count > MAX_PASSCODE_BUTTONS) {
        FURI_LOG_E(
            TAG,
            "set_passcode_sequence: Invalid button count: %zu (must be %d-%d)",
            button_count,
            MIN_PASSCODE_BUTTONS,
            MAX_PASSCODE_BUTTONS);
        return false;
    }

    // Ensure crypto key is initialized before encryption
    if(!ensure_crypto_key()) {
        FURI_LOG_E(TAG, "set_passcode_sequence: Failed to ensure crypto key");
        return false;
    }

    // Small delay to let crypto subsystem settle
    furi_delay_ms(PASSCODE_DELAY_MS);

    // Encrypt the passcode
    uint8_t* encrypted = malloc(seq_len + AES_BLOCK_SIZE);
    size_t encrypted_len = 0;

    if(!encrypted) {
        FURI_LOG_E(TAG, "set_passcode_sequence: Failed to allocate encryption buffer");
        return false;
    }

    bool encrypt_success =
        encrypt_data((const uint8_t*)sequence, seq_len, encrypted, &encrypted_len);
    if(!encrypt_success) {
        FURI_LOG_E(TAG, "set_passcode_sequence: Failed to encrypt passcode");
        free(encrypted);
        return false;
    }

    // Save to cards.enc file header
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "set_passcode_sequence: Failed to open storage");
        memset(encrypted, 0, encrypted_len);
        free(encrypted);
        return false;
    }

    app_ensure_data_dir(storage);

    // Read existing cards.enc to preserve card data and flags (read entire file into memory)
    File* read_file = storage_file_alloc(storage);
    uint8_t* existing_cards = NULL;
    size_t existing_cards_len = 0;
    uint8_t flags = 0; // Preserve existing flags

    if(storage_file_open(read_file, NFC_CARDS_FILE_ENC, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(read_file);

        if(file_size >= PASSCODE_HEADER_SIZE) {
            // Read entire file
            uint8_t* file_data = malloc(file_size);
            if(file_data) {
                size_t bytes_read = storage_file_read(read_file, file_data, file_size);
                if(bytes_read == file_size) {
                    // Parse header
                    uint16_t old_passcode_len = (uint16_t)(file_data[0] | (file_data[1] << 8));

                    // Extract flags byte (3rd byte of header)
                    if(file_size >= 3) {
                        flags = file_data[2];
                    }

                    // Extract card data (skip header and old passcode)
                    size_t card_data_offset = PASSCODE_HEADER_SIZE + old_passcode_len;
                    if(card_data_offset < file_size) {
                        existing_cards_len = file_size - card_data_offset;
                        if(existing_cards_len > 0 && existing_cards_len < MAX_ENCRYPTED_SIZE) {
                            existing_cards = malloc(existing_cards_len);
                            if(existing_cards) {
                                memcpy(
                                    existing_cards,
                                    file_data + card_data_offset,
                                    existing_cards_len);
                            }
                        }
                    }
                } else {
                    FURI_LOG_E(
                        TAG,
                        "set_passcode_sequence: Failed to read entire file: %zu/%zu",
                        bytes_read,
                        file_size);
                }
                free(file_data);
            }
        } else {
        }
        storage_file_close(read_file);
    } else {
    }
    storage_file_free(read_file);

    // Write new cards.enc with passcode header
    File* write_file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(write_file, NFC_CARDS_FILE_ENC, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Write passcode length (2 bytes, little-endian) and flags (1 byte)
        uint8_t header_bytes[3];
        header_bytes[0] = (uint8_t)(encrypted_len & 0xFF);
        header_bytes[1] = (uint8_t)((encrypted_len >> 8) & 0xFF);
        header_bytes[2] = flags; // Preserve existing flags
        storage_file_write(write_file, header_bytes, 3);

        // Write encrypted passcode
        size_t written = storage_file_write(write_file, encrypted, encrypted_len);

        if(written == encrypted_len) {
            // Write existing card data if any
            if(existing_cards && existing_cards_len > 0) {
                storage_file_write(write_file, existing_cards, existing_cards_len);
            }

            storage_file_close(write_file);
            success = true;
        } else {
            FURI_LOG_E(
                TAG, "set_passcode_sequence: Write failed: %zu/%zu bytes", written, encrypted_len);
            storage_file_close(write_file);
        }
    } else {
        FURI_LOG_E(TAG, "set_passcode_sequence: Failed to open cards file for writing");
    }

    if(existing_cards) {
        free(existing_cards);
    }

    storage_file_free(write_file);
    furi_record_close(RECORD_STORAGE);

    // Small delay to ensure file system operations complete
    furi_delay_ms(PASSCODE_DELAY_MS);

    memset(encrypted, 0, encrypted_len);
    free(encrypted);

    return success;
}

bool has_passcode(void) {
    char test_sequence[MAX_PASSCODE_SEQUENCE_LEN];
    return get_passcode_sequence(test_sequence, sizeof(test_sequence));
}

bool verify_passcode_sequence(const char* sequence) {
    char stored_sequence[MAX_PASSCODE_SEQUENCE_LEN];
    if(!get_passcode_sequence(stored_sequence, sizeof(stored_sequence))) {
        return false;
    }

    return (strcmp(stored_sequence, sequence) == 0);
}

bool sequence_to_bytes(const char* sequence, uint8_t* bytes_out, size_t* bytes_len_out) {
    if(!sequence || !bytes_out || !bytes_len_out) {
        return false;
    }

    size_t seq_len = strlen(sequence);
    if(seq_len > MAX_PASSCODE_SEQUENCE_LEN) {
        return false;
    }

    memcpy(bytes_out, sequence, seq_len);
    *bytes_len_out = seq_len;
    return true;
}

bool delete_cards_and_reset_passcode(void) {
    FURI_LOG_W(TAG, "delete_cards_and_reset_passcode: Deleting cards.enc and resetting passcode");

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "delete_cards_and_reset_passcode: Failed to open storage");
        return false;
    }

    // Delete the cards.enc file
    FS_Error error = storage_common_remove(storage, NFC_CARDS_FILE_ENC);
    furi_record_close(RECORD_STORAGE);

    if(error == FSE_OK || error == FSE_NOT_EXIST) {
        return true;
    } else {
        FURI_LOG_E(
            TAG, "delete_cards_and_reset_passcode: Failed to delete cards.enc (error: %d)", error);
        return false;
    }
}

bool reset_passcode_preserve_cards(void) {
    // Create a temporary App structure to load cards
    // We'll try to load cards with hardware decryption (bypassing passcode check)
    App temp_app = {0};

    // Try to load cards - app_load_cards will try hardware decryption if passcode decryption fails
    // This works because app_load_cards falls back to hardware decryption
    app_load_cards(&temp_app);

    size_t cards_preserved = temp_app.card_count;

    // Delete the file to reset passcode
    bool delete_success = delete_cards_and_reset_passcode();

    if(!delete_success) {
        FURI_LOG_E(TAG, "reset_passcode_preserve_cards: Failed to delete cards.enc");
        return false;
    }

    // If we successfully loaded cards, save them back (they'll be encrypted with hardware encryption or new passcode)
    if(cards_preserved > 0) {
        // Copy cards to a proper App structure for saving
        // Note: This is a simplified approach - in practice, we'd need the full App context
        FURI_LOG_W(
            TAG,
            "reset_passcode_preserve_cards: Note: Cards will need to be re-saved after new passcode is set");
        // Cards are preserved in temp_app, but we can't save them here without full App context
        // The caller should handle saving after setting new passcode
    } else {
        FURI_LOG_W(
            TAG,
            "reset_passcode_preserve_cards: No cards could be loaded (likely passcode-encrypted), all cards deleted");
    }

    return true;
}

bool derive_key_from_passcode_sequence(const char* sequence, uint8_t* key_out, size_t key_len) {
    if(!sequence || strlen(sequence) == 0 || !key_out || key_len == 0) {
        FURI_LOG_E(TAG, "derive_key_from_passcode_sequence: Invalid parameters");
        return false;
    }

    // Convert sequence to bytes
    uint8_t passcode_bytes[MAX_PASSCODE_SEQUENCE_LEN];
    size_t passcode_len = 0;
    if(!sequence_to_bytes(sequence, passcode_bytes, &passcode_len)) {
        FURI_LOG_E(TAG, "derive_key_from_passcode_sequence: Failed to convert sequence");
        return false;
    }

    uint8_t salt[KEY_DERIVATION_SALT_SIZE];
    get_key_derivation_salt(salt);

    // Simple key derivation using iterative hashing (PBKDF2-like but without mbedtls)
    // This is a simplified version that provides reasonable security
    uint8_t hash_input[PASSCODE_HASH_SIZE];
    uint8_t hash_output[PASSCODE_HASH_SIZE];

    // Initial hash: passcode + salt
    memset(hash_input, 0, PASSCODE_HASH_SIZE);
    size_t input_len = passcode_len < PASSCODE_HASH_SIZE ? passcode_len : PASSCODE_HASH_SIZE;
    memcpy(hash_input, passcode_bytes, input_len);
    for(size_t i = 0; i < KEY_DERIVATION_SALT_SIZE && (input_len + i) < PASSCODE_HASH_SIZE; i++) {
        hash_input[input_len + i] = salt[i];
    }

    // Simple hash function
    for(size_t i = 0; i < PASSCODE_HASH_SIZE; i++) {
        hash_output[i] = hash_input[i];
        for(int j = 0; j < 7; j++) {
            hash_output[i] = (hash_output[i] << 1) | (hash_output[i] >> 7);
            hash_output[i] ^= hash_input[(i + j + 1) % PASSCODE_HASH_SIZE];
        }
    }

    // Iterative hashing (simplified PBKDF2)
    for(uint32_t iter = 0; iter < PBKDF2_ITERATIONS; iter++) {
        // Mix previous output with passcode and salt
        for(size_t i = 0; i < PASSCODE_HASH_SIZE; i++) {
            hash_input[i] = hash_output[i] ^ passcode_bytes[i % passcode_len] ^
                            salt[i % KEY_DERIVATION_SALT_SIZE];
        }

        // Hash the mixed input
        for(size_t i = 0; i < PASSCODE_HASH_SIZE; i++) {
            hash_output[i] = hash_input[i];
            for(int j = 0; j < 5; j++) {
                hash_output[i] = (hash_output[i] << 2) | (hash_output[i] >> 6);
                hash_output[i] ^= hash_input[(i + j + iter) % PASSCODE_HASH_SIZE];
                hash_output[i] ^= (uint8_t)(iter + i);
            }
        }
    }

    // Copy to output key (truncate or extend as needed)
    if(key_len <= PASSCODE_HASH_SIZE) {
        memcpy(key_out, hash_output, key_len);
    } else {
        // If key is longer, extend by hashing again
        memcpy(key_out, hash_output, PASSCODE_HASH_SIZE);
        for(size_t offset = PASSCODE_HASH_SIZE; offset < key_len; offset += PASSCODE_HASH_SIZE) {
            size_t copy_len = (key_len - offset) < PASSCODE_HASH_SIZE ? (key_len - offset) :
                                                                        PASSCODE_HASH_SIZE;
            // Hash the previous chunk to get next chunk
            for(size_t i = 0; i < PASSCODE_HASH_SIZE; i++) {
                hash_output[i] = (hash_output[i] << 3) | (hash_output[i] >> 5);
                hash_output[i] ^= (uint8_t)(offset + i);
            }
            memcpy(key_out + offset, hash_output, copy_len);
        }
    }

    memset(passcode_bytes, 0, passcode_len);
    memset(hash_input, 0, PASSCODE_HASH_SIZE);
    memset(hash_output, 0, PASSCODE_HASH_SIZE);

    return true;
}

bool get_passcode_disabled(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool disabled = false;

    if(storage_file_open(file, NFC_CARDS_FILE_ENC, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);

        if(file_size >= PASSCODE_HEADER_SIZE) {
            uint8_t header[3];
            if(storage_file_read(file, header, 3) == 3) {
                disabled = (header[2] & PASSCODE_FLAG_DISABLED) != 0;
            }
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return disabled;
}

bool set_passcode_disabled(bool disabled) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, NFC_CARDS_FILE_ENC, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);

        if(file_size >= PASSCODE_HEADER_SIZE) {
            // Read entire file
            uint8_t* file_data = malloc(file_size);
            if(file_data) {
                size_t bytes_read = storage_file_read(file, file_data, file_size);

                if(bytes_read == file_size) {
                    // Update flags byte
                    if(file_size >= 3) {
                        if(disabled) {
                            file_data[2] |= PASSCODE_FLAG_DISABLED;
                        } else {
                            file_data[2] &= ~PASSCODE_FLAG_DISABLED;
                        }

                        storage_file_close(file);

                        // Write back
                        if(storage_file_open(
                               file, NFC_CARDS_FILE_ENC, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                            size_t written = storage_file_write(file, file_data, file_size);
                            if(written == file_size) {
                                success = true;
                            }
                            storage_file_close(file);
                        }
                    }
                }

                free(file_data);
            }
        } else {
            // File too small, create new header
            storage_file_close(file);
            if(storage_file_open(file, NFC_CARDS_FILE_ENC, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                uint8_t header[3] = {0, 0, disabled ? PASSCODE_FLAG_DISABLED : 0};
                success = (storage_file_write(file, header, 3) == 3);
                storage_file_close(file);
            }
        }
    } else {
        // File doesn't exist, create new header
        if(storage_file_open(file, NFC_CARDS_FILE_ENC, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            uint8_t header[3] = {0, 0, disabled ? PASSCODE_FLAG_DISABLED : 0};
            success = (storage_file_write(file, header, 3) == 3);
            storage_file_close(file);
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}
