#include "nfc_login_card_storage.h"
#include "../crypto/nfc_login_crypto.h"
#include "../crypto/nfc_login_passcode.h"
#include <storage/storage.h>
#include <furi.h>
#include <ctype.h>

static void uid_to_hex(const uint8_t* uid, size_t uid_len, char* hex_out) {
    for(size_t i = 0; i < uid_len; i++) {
        snprintf(hex_out + i * 2, 3, "%02X", uid[i]);
    }
    hex_out[uid_len * 2] = '\0';
}

static bool parse_card_line(const char* line, NfcCard* card) {
    char line_copy[128];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';

    char name[32] = {0};
    char uid_hex[32] = {0};
    char password[64] = {0};

    uint8_t part = 0;
    size_t part_start = 0;

    for(size_t i = 0; line_copy[i] != '\0' && line_copy[i] != '\n' && line_copy[i] != '\r'; i++) {
        if(line_copy[i] == '|') {
            size_t part_len = i - part_start;
            char* dest = NULL;
            size_t dest_size = 0;

            if(part == 0) {
                dest = name;
                dest_size = sizeof(name);
            } else if(part == 1) {
                dest = uid_hex;
                dest_size = sizeof(uid_hex);
            }

            if(dest && part_len < dest_size) {
                strncpy(dest, &line_copy[part_start], part_len);
                dest[part_len] = '\0';
            }

            part_start = i + 1;
            part++;
            if(part >= 2) break;
        }
    }

    if(part == 2) {
        size_t part_len = 0;
        size_t i = part_start;
        while(line_copy[i] != '\0' && line_copy[i] != '\n' && line_copy[i] != '\r') {
            part_len++;
            i++;
        }
        while(part_len > 0 && (line_copy[part_start + part_len - 1] == '\r' ||
                               line_copy[part_start + part_len - 1] == '\n')) {
            part_len--;
        }
        if(part_len < sizeof(password)) {
            strncpy(password, &line_copy[part_start], part_len);
            password[part_len] = '\0';
        }
    }

    if(name[0] != '\0' && uid_hex[0] != '\0' && password[0] != '\0') {
        memset(card, 0, sizeof(NfcCard));
        strncpy(card->name, name, sizeof(card->name) - 1);
        strncpy(card->password, password, sizeof(card->password) - 1);

        size_t uid_hex_len = strlen(uid_hex);
        size_t uid_len = uid_hex_len / 2;
        if(uid_len > 0 && uid_len <= MAX_UID_LEN) {
            card->uid_len = uid_len;
            for(size_t i = 0; i < uid_len; i++) {
                unsigned int byte_val = 0;
                sscanf(uid_hex + i * 2, "%2x", &byte_val);
                card->uid[i] = (uint8_t)byte_val;
            }
            return true;
        }
    }

    return false;
}

void app_ensure_data_dir(Storage* storage) {
    storage_common_mkdir(storage, APP_DATA_DIR);
}

bool app_save_cards(App* app) {
    size_t estimated_size = app->card_count * 256;
    if(estimated_size < 512) estimated_size = 512;
    if(estimated_size > MAX_ENCRYPTED_SIZE) estimated_size = MAX_ENCRYPTED_SIZE;

    char* plaintext = malloc(estimated_size);
    if(!plaintext) {
        FURI_LOG_E(TAG, "app_save_cards: Failed to allocate plaintext buffer");
        return false;
    }

    size_t plaintext_len = 0;
    for(size_t i = 0; i < app->card_count; i++) {
        char line[128];
        char uid_hex[MAX_UID_LEN * 2 + 1]; // uid_to_hex null-terminates, no need to initialize

        uid_to_hex(app->cards[i].uid, app->cards[i].uid_len, uid_hex);

        int written = snprintf(
            line, sizeof(line), "%s|%s|%s\n", app->cards[i].name, uid_hex, app->cards[i].password);
        if(written > 0 && plaintext_len + written < estimated_size) {
            memcpy(plaintext + plaintext_len, line, written);
            plaintext_len += written;
        }
    }

    uint8_t* encrypted = NULL;
    size_t encrypted_len = 0;
    bool encryption_success = false;

    if(plaintext_len > 0) {
        encrypted = malloc(estimated_size + AES_BLOCK_SIZE);
        if(!encrypted) {
            FURI_LOG_E(TAG, "app_save_cards: Failed to allocate encrypted buffer");
            memset(plaintext, 0, estimated_size);
            free(plaintext);
            return false;
        }

        if(has_passcode()) {
            char passcode_sequence[MAX_PASSCODE_SEQUENCE_LEN];
            if(get_passcode_sequence(passcode_sequence, sizeof(passcode_sequence))) {
                encryption_success = encrypt_data_with_passcode_sequence(
                    (uint8_t*)plaintext,
                    plaintext_len,
                    encrypted,
                    &encrypted_len,
                    passcode_sequence);
                memset(passcode_sequence, 0, sizeof(passcode_sequence));
            } else {
                FURI_LOG_W(
                    TAG,
                    "app_save_cards: Failed to get passcode, falling back to hardware encryption");
                encryption_success =
                    encrypt_data((uint8_t*)plaintext, plaintext_len, encrypted, &encrypted_len);
            }
        } else {
            encryption_success =
                encrypt_data((uint8_t*)plaintext, plaintext_len, encrypted, &encrypted_len);
        }

        if(!encryption_success) {
            FURI_LOG_E(TAG, "app_save_cards: Encryption failed");
            memset(plaintext, 0, estimated_size);
            memset(encrypted, 0, estimated_size + AES_BLOCK_SIZE);
            free(plaintext);
            free(encrypted);
            return false;
        }

        memset(plaintext, 0, estimated_size);
        free(plaintext);
        plaintext = NULL;

        furi_delay_ms(CRYPTO_SETTLE_DELAY_MS);
    } else if(app->card_count == 0) {
        memset(plaintext, 0, estimated_size);
        free(plaintext);
        plaintext = NULL;
    }

    bool success = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "app_save_cards: Failed to open storage record");
        if(encrypted) {
            memset(encrypted, 0, encrypted_len);
            free(encrypted);
        }
        return false;
    }

    FS_Error sd_status = storage_sd_status(storage);
    if(sd_status != FSE_OK) {
        FURI_LOG_E(TAG, "app_save_cards: SD not ready (status=%d)", sd_status);
        furi_record_close(RECORD_STORAGE);
        if(encrypted) {
            memset(encrypted, 0, encrypted_len);
            free(encrypted);
        }
        return false;
    }

    app_ensure_data_dir(storage);

    static uint8_t default_header[PASSCODE_HEADER_SIZE] = {0, 0, 0};

    uint8_t* passcode_header = NULL;
    size_t passcode_header_len = 0;

    File* read_file = storage_file_alloc(storage);
    if(storage_file_open(read_file, NFC_CARDS_FILE_ENC, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(read_file);

        if(file_size >= PASSCODE_HEADER_SIZE) {
            uint8_t* file_data = malloc(file_size);
            if(file_data) {
                size_t bytes_read = storage_file_read(read_file, file_data, file_size);
                if(bytes_read == file_size) {
                    uint16_t passcode_len = (uint16_t)(file_data[0] | (file_data[1] << 8));

                    if(passcode_len > 0 && passcode_len < 512) {
                        passcode_header = malloc(PASSCODE_HEADER_SIZE + passcode_len);
                        if(passcode_header) {
                            memcpy(
                                passcode_header, file_data, PASSCODE_HEADER_SIZE + passcode_len);
                            passcode_header_len = PASSCODE_HEADER_SIZE + passcode_len;
                        }
                    } else {
                        passcode_header = malloc(PASSCODE_HEADER_SIZE);
                        if(passcode_header) {
                            memcpy(passcode_header, default_header, PASSCODE_HEADER_SIZE);
                            passcode_header_len = PASSCODE_HEADER_SIZE;
                        }
                    }
                }
                free(file_data);
            }
        } else {
            passcode_header = malloc(PASSCODE_HEADER_SIZE);
            if(passcode_header) {
                memcpy(passcode_header, default_header, PASSCODE_HEADER_SIZE);
                passcode_header_len = PASSCODE_HEADER_SIZE;
            }
        }
        storage_file_close(read_file);
    } else {
        passcode_header = malloc(PASSCODE_HEADER_SIZE);
        if(passcode_header) {
            memcpy(passcode_header, default_header, PASSCODE_HEADER_SIZE);
            passcode_header_len = PASSCODE_HEADER_SIZE;
        }
    }
    storage_file_free(read_file);

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E(TAG, "app_save_cards: Failed to allocate file object");
        furi_record_close(RECORD_STORAGE);
        if(encrypted) {
            memset(encrypted, 0, encrypted_len);
            free(encrypted);
        }
        if(passcode_header) {
            free(passcode_header);
        }
        return false;
    }

    bool file_opened = storage_file_open(file, NFC_CARDS_FILE_ENC, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!file_opened) {
        FURI_LOG_E(TAG, "app_save_cards: Failed to open file for writing");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        if(encrypted) {
            memset(encrypted, 0, encrypted_len);
            free(encrypted);
        }
        if(passcode_header) {
            free(passcode_header);
        }
        return false;
    }

    if(encryption_success && encrypted_len > 0) {
        if(passcode_header) {
            storage_file_write(file, passcode_header, passcode_header_len);
        }

        size_t written = storage_file_write(file, encrypted, encrypted_len);
        if(written == encrypted_len) {
            success = true;
        } else {
            FURI_LOG_E(TAG, "app_save_cards: Write failed: %zu/%zu bytes", written, encrypted_len);
        }
    } else if(app->card_count == 0) {
        if(passcode_header) {
            storage_file_write(file, passcode_header, passcode_header_len);
        } else {
            uint8_t header[3] = {0, 0, 0};
            storage_file_write(file, header, 3);
        }
        success = true;
    }

    if(passcode_header) {
        free(passcode_header);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(encrypted) {
        memset(encrypted, 0, encrypted_len);
        free(encrypted);
    }

    return success;
}

void app_load_cards(App* app) {
    app->card_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "app_load_cards: Failed to open storage record");
        return;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E(TAG, "app_load_cards: Failed to allocate file object");
        furi_record_close(RECORD_STORAGE);
        return;
    }

    uint8_t* encrypted = NULL;
    size_t encrypted_len = 0;
    bool file_read_success = false;

    if(storage_file_open(file, NFC_CARDS_FILE_ENC, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);

        if(file_size < PASSCODE_HEADER_SIZE) {
            FURI_LOG_E(TAG, "app_load_cards: File too small for header: %zu", file_size);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }

        uint8_t header[3];
        size_t bytes_read = storage_file_read(file, header, 3);
        if(bytes_read < 2) {
            FURI_LOG_E(TAG, "app_load_cards: Failed to read passcode header");
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }

        uint16_t passcode_len = (uint16_t)(header[0] | (header[1] << 8));

        uint8_t* file_data = malloc(file_size);
        if(!file_data) {
            FURI_LOG_E(TAG, "app_load_cards: Failed to allocate file buffer");
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }

        storage_file_seek(file, 0, true);
        size_t total_read = storage_file_read(file, file_data, file_size);

        if(total_read != file_size) {
            FURI_LOG_E(
                TAG, "app_load_cards: Failed to read entire file: %zu/%zu", total_read, file_size);
            free(file_data);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }

        size_t cards_data_size = file_size - PASSCODE_HEADER_SIZE - passcode_len;

        if(cards_data_size == 0) {
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            app->card_count = 0;
            return;
        }

        if(cards_data_size > MAX_ENCRYPTED_SIZE) {
            FURI_LOG_E(TAG, "app_load_cards: Card data too large: %zu", cards_data_size);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }

        if(cards_data_size % AES_BLOCK_SIZE != 0) {
            FURI_LOG_E(
                TAG, "app_load_cards: Invalid card data size (not aligned): %zu", cards_data_size);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }

        encrypted = malloc(cards_data_size);
        if(!encrypted) {
            FURI_LOG_E(TAG, "app_load_cards: Failed to allocate encrypted buffer");
            free(file_data);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return;
        }

        size_t card_data_offset = PASSCODE_HEADER_SIZE + passcode_len;
        memcpy(encrypted, file_data + card_data_offset, cards_data_size);

        free(file_data);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);

        encrypted_len = cards_data_size;
        file_read_success = true;
    } else {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        app->card_count = 0;
        return;
    }

    if(file_read_success && encrypted_len > 0) {
        char* plaintext = malloc(MAX_ENCRYPTED_SIZE);
        if(!plaintext) {
            FURI_LOG_E(TAG, "app_load_cards: Failed to allocate plaintext buffer");
            memset(encrypted, 0, encrypted_len);
            free(encrypted);
            return;
        }

        size_t plaintext_len = 0;
        bool decrypt_success = false;

        if(has_passcode()) {
            char passcode_sequence[MAX_PASSCODE_SEQUENCE_LEN];
            if(get_passcode_sequence(passcode_sequence, sizeof(passcode_sequence))) {
                decrypt_success = decrypt_data_with_passcode_sequence(
                    encrypted,
                    encrypted_len,
                    (uint8_t*)plaintext,
                    &plaintext_len,
                    passcode_sequence);
                memset(passcode_sequence, 0, sizeof(passcode_sequence));

                if(!decrypt_success) {
                    FURI_LOG_W(
                        TAG,
                        "app_load_cards: Passcode decryption failed, trying hardware decryption");
                }
            } else {
                FURI_LOG_W(
                    TAG,
                    "app_load_cards: Passcode exists but failed to get sequence, trying hardware decryption");
            }
        }

        // Fall back to hardware decryption if passcode decryption failed or no passcode
        if(!decrypt_success) {
            if(!ensure_crypto_key()) {
                FURI_LOG_E(TAG, "app_load_cards: Failed to ensure crypto key");
                memset(encrypted, 0, encrypted_len);
                memset(plaintext, 0, MAX_ENCRYPTED_SIZE);
                free(encrypted);
                free(plaintext);
                return;
            }

            decrypt_success =
                decrypt_data(encrypted, encrypted_len, (uint8_t*)plaintext, &plaintext_len);
        }

        memset(encrypted, 0, encrypted_len);
        free(encrypted);
        encrypted = NULL;

        if(!decrypt_success) {
            FURI_LOG_E(TAG, "app_load_cards: Decryption failed");
            memset(plaintext, 0, MAX_ENCRYPTED_SIZE);
            free(plaintext);
            return;
        }
        furi_delay_ms(STORAGE_READ_DELAY_MS);

        char* line_start = plaintext;
        while(app->card_count < MAX_CARDS && line_start < plaintext + plaintext_len) {
            char* line_end = strchr(line_start, '\n');
            if(!line_end) {
                line_end = plaintext + plaintext_len;
            }

            size_t line_len = line_end - line_start;
            if(line_len > 0 && line_len < 128) {
                char line[128];
                memcpy(line, line_start, line_len);
                line[line_len] = '\0';

                if(line_len > 0 && line[line_len - 1] == '\r') {
                    line[line_len - 1] = '\0';
                }

                if(line[0] != '\0') {
                    if(parse_card_line(line, &app->cards[app->card_count])) {
                        app->card_count++;
                    }
                }
            }

            if(line_end >= plaintext + plaintext_len) break;
            line_start = line_end + 1;
        }

        memset(plaintext, 0, plaintext_len);
        free(plaintext);
    }

    if(app->active_card_index < app->card_count && app->card_count > 0) {
        app->has_active_selection = true;
        app->selected_card = app->active_card_index;
    } else if(app->active_card_index > 0) {
        app->has_active_selection = false;
        app->active_card_index = 0;
        FURI_LOG_W(
            TAG,
            "app_load_cards: Saved card index no longer exists (card_count=%zu), clearing selection",
            app->card_count);
    }
}
