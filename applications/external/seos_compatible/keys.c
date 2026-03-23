#include "seos_i.h"

#define TAG "SeosKeys"

size_t SEOS_ADF_OID_LEN = 9;
uint8_t SEOS_ADF_OID[32] = {0x03, 0x01, 0x07, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t SEOS_ADF1_PRIV_ENC[] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t SEOS_ADF1_PRIV_MAC[] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t SEOS_ADF1_READ[] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t SEOS_ADF1_WRITE[] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void seos_reset_to_zero_keys(Seos* seos) {
    SEOS_ADF_OID_LEN = 9;
    memset(SEOS_ADF_OID, 0, sizeof(SEOS_ADF_OID));
    SEOS_ADF_OID[0] = 0x03;
    SEOS_ADF_OID[1] = 0x01;
    SEOS_ADF_OID[2] = 0x07;
    SEOS_ADF_OID[3] = 0x09;
    memset(SEOS_ADF1_PRIV_ENC, 0, 16);
    memset(SEOS_ADF1_PRIV_MAC, 0, 16);
    memset(SEOS_ADF1_READ, 0, 16);
    memset(SEOS_ADF1_WRITE, 0, 16);
    seos->keys_version = 0;
    furi_string_reset(seos->active_key_file);
}

bool seos_migrate_keys(Seos* seos) {
    const char* file_header = "Seos keys";
    const uint32_t file_version = 2;
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(seos->credential->storage);
    FuriString* path = furi_string_alloc();
    FuriString* temp_str = furi_string_alloc();
    uint8_t iv[16] = {0};
    memset(iv, 0, sizeof(iv));
    uint8_t output[16];

    if(seos->keys_version == 0) {
        FURI_LOG_E(TAG, "Keys not loaded, can't migrate");
        return false;
    }
    if(seos->keys_version == 2) {
        FURI_LOG_I(TAG, "Keys already migrated to version 2");
        return false; // Already migrated
    }

    do {
        // Check for valid filename first to fail fast
        if(furi_string_empty(seos->active_key_file)) break;

        furi_string_printf(
            path,
            "%s/%s%s",
            STORAGE_APP_DATA_PATH_PREFIX,
            furi_string_get_cstr(seos->active_key_file),
            ".txt");

        // Encrypt the keys using the per-device key
        if(!furi_hal_crypto_enclave_ensure_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT)) {
            FURI_LOG_E(TAG, "Failed to ensure unique key slot");
            break;
        }
        if(!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
            FURI_LOG_E(TAG, "Failed to load unique key");
            break;
        }

        // Open file
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) break;
        if(!flipper_format_write_header_cstr(file, file_header, file_version)) break;
        if(!flipper_format_write_uint32(file, "SEOS_ADF_OID_LEN", (uint32_t*)&SEOS_ADF_OID_LEN, 1))
            break;
        if(!flipper_format_write_hex(file, "SEOS_ADF_OID", SEOS_ADF_OID, SEOS_ADF_OID_LEN)) break;

        if(furi_hal_crypto_encrypt(SEOS_ADF1_PRIV_ENC, output, sizeof(output))) {
            if(!flipper_format_write_hex(file, "SEOS_ADF1_PRIV_ENC", output, 16)) break;
        }

        if(furi_hal_crypto_encrypt(SEOS_ADF1_PRIV_MAC, output, sizeof(output))) {
            if(!flipper_format_write_hex(file, "SEOS_ADF1_PRIV_MAC", output, 16)) break;
        }
        if(furi_hal_crypto_encrypt(SEOS_ADF1_READ, output, sizeof(output))) {
            if(!flipper_format_write_hex(file, "SEOS_ADF1_READ", output, 16)) break;
        }
        if(furi_hal_crypto_encrypt(SEOS_ADF1_WRITE, output, sizeof(output))) {
            if(!flipper_format_write_hex(file, "SEOS_ADF1_WRITE", output, 16)) break;
        }

        if(!furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT)) {
            FURI_LOG_E(TAG, "Failed to unload unique key");
            // No break since we've already decrypted
        }

        parsed = true;
        seos->keys_version = file_version;
    } while(false);

    if(parsed) {
        FURI_LOG_I(TAG, "Keys migrated to V%d", seos->keys_version);
    }

    furi_string_free(path);
    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}

// Version 2 has keys that are encrypted using the per-device key in the secure enclave
static bool seos_load_keys_v2(Seos* seos, const char* filename) {
    const char* file_header = "Seos keys";
    const uint32_t file_version = 2;
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(seos->credential->storage);
    FuriString* path = furi_string_alloc();
    FuriString* temp_str = furi_string_alloc();
    uint32_t version = 0;
    uint8_t iv[16] = {0};
    memset(iv, 0, sizeof(iv));
    uint8_t output[16];

    do {
        furi_string_printf(path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, filename, ".txt");
        // Open file
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(!furi_string_equal_str(temp_str, file_header) || (version != file_version)) {
            break;
        }

        if(!flipper_format_read_uint32(file, "SEOS_ADF_OID_LEN", (uint32_t*)&SEOS_ADF_OID_LEN, 1))
            break;
        if(!flipper_format_read_hex(file, "SEOS_ADF_OID", SEOS_ADF_OID, SEOS_ADF_OID_LEN)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_PRIV_ENC", SEOS_ADF1_PRIV_ENC, 16)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_PRIV_MAC", SEOS_ADF1_PRIV_MAC, 16)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_READ", SEOS_ADF1_READ, 16)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_WRITE", SEOS_ADF1_WRITE, 16)) break;

        // Decrypt the keys using the per-device key
        if(!furi_hal_crypto_enclave_ensure_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT)) {
            FURI_LOG_E(TAG, "Failed to ensure unique key slot");
            break;
        }
        if(!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
            FURI_LOG_E(TAG, "Failed to load unique key");
            break;
        }

        if(furi_hal_crypto_decrypt(SEOS_ADF1_PRIV_ENC, output, sizeof(output))) {
            memcpy(SEOS_ADF1_PRIV_ENC, output, sizeof(SEOS_ADF1_PRIV_ENC));
        }
        if(furi_hal_crypto_decrypt(SEOS_ADF1_PRIV_MAC, output, sizeof(output))) {
            memcpy(SEOS_ADF1_PRIV_MAC, output, sizeof(SEOS_ADF1_PRIV_MAC));
        }
        if(furi_hal_crypto_decrypt(SEOS_ADF1_READ, output, sizeof(output))) {
            memcpy(SEOS_ADF1_READ, output, sizeof(SEOS_ADF1_READ));
        }
        if(furi_hal_crypto_decrypt(SEOS_ADF1_WRITE, output, sizeof(output))) {
            memcpy(SEOS_ADF1_WRITE, output, sizeof(SEOS_ADF1_WRITE));
        }

        if(!furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT)) {
            FURI_LOG_E(TAG, "Failed to unload unique key");
            // No break since we've already decrypted
        }

        parsed = true;
        seos->keys_version = file_version;
    } while(false);

    if(parsed) {
        FURI_LOG_I(TAG, "Keys loaded V%d", seos->keys_version);
        seos_log_buffer(TAG, "Keys for ADF OID loaded", SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    } else {
        FURI_LOG_I(TAG, "V2: Parsing failed");
    }

    furi_string_free(path);
    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}

static bool seos_load_keys_v1(Seos* seos, const char* filename) {
    const char* file_header = "Seos keys";
    const uint32_t file_version = 1;
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(seos->credential->storage);
    FuriString* path = furi_string_alloc();
    FuriString* temp_str = furi_string_alloc();
    uint32_t version = 0;

    do {
        furi_string_printf(path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, filename, ".txt");
        // Open file
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(!furi_string_equal_str(temp_str, file_header) || (version != file_version)) {
            break;
        }

        if(!flipper_format_read_uint32(file, "SEOS_ADF_OID_LEN", (uint32_t*)&SEOS_ADF_OID_LEN, 1))
            break;
        if(!flipper_format_read_hex(file, "SEOS_ADF_OID", SEOS_ADF_OID, SEOS_ADF_OID_LEN)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_PRIV_ENC", SEOS_ADF1_PRIV_ENC, 16)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_PRIV_MAC", SEOS_ADF1_PRIV_MAC, 16)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_READ", SEOS_ADF1_READ, 16)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_WRITE", SEOS_ADF1_WRITE, 16)) break;

        parsed = true;
        seos->keys_version = file_version;
    } while(false);

    if(parsed) {
        FURI_LOG_I(TAG, "Keys loaded V%d", seos->keys_version);
        seos_log_buffer(TAG, "Keys for ADF OID loaded", SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    } else {
        FURI_LOG_I(TAG, "V1: Parsing failed");
    }

    furi_string_free(path);
    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}

bool seos_load_keys_from_file(Seos* seos, const char* filename) {
    if(seos_load_keys_v2(seos, filename) || seos_load_keys_v1(seos, filename)) {
        furi_string_set_str(seos->active_key_file, filename);
        return true;
    }
    // Ensure keys are in a known state after a failed load
    seos_reset_to_zero_keys(seos);
    return false;
}
