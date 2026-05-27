/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

// cppcheck-suppress-file variableScope
// cppcheck-suppress-file unreadVariable

#include <furi.h>
#include "persistence.h"
#include <furi_hal.h>
#include <storage/storage.h>
#include <furi_hal_random.h>
#include <flipper_format/flipper_format.h>
#include <stdlib.h>
#include "common.h"
#include "../attestation/local.h"
#include "../zerofido_crypto.h"
#include "../zerofido_storage.h"
#include "../zerofido_types.h"

#define TAG "U2f"

#if !ZF_RELEASE_DIAGNOSTICS
#undef FURI_LOG_E
#undef FURI_LOG_I
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)
#endif

#define U2F_DATA_FOLDER ZF_APP_DATA_DIR "/u2f"
#define U2F_ASSETS_FOLDER U2F_DATA_FOLDER "/assets"
#define U2F_CERT_FILE U2F_DATA_FOLDER "/assets/cert.der"
#define U2F_CERT_FILE_TMP U2F_DATA_FOLDER "/assets/cert.der.tmp"
#define U2F_CERT_KEY_FILE U2F_DATA_FOLDER "/assets/cert_key.u2f"
#define U2F_CERT_KEY_FILE_TMP U2F_DATA_FOLDER "/assets/cert_key.u2f.tmp"
#define U2F_KEY_FILE U2F_DATA_FOLDER "/key.u2f"
#define U2F_KEY_FILE_TMP U2F_DATA_FOLDER "/key.u2f.tmp"
#define U2F_CNT_FILE U2F_DATA_FOLDER "/cnt.u2f"
#define U2F_CNT_FILE_TMP U2F_DATA_FOLDER "/cnt.u2f.tmp"

/*
 * U2F persistence uses FlipperFormat wrappers with encrypted binary payloads.
 * key.u2f is additionally authenticated with an HMAC derived from the plaintext
 * device key to detect file tampering after decryption.
 */

#define U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT

#define U2F_CERT_USER 1

#define U2F_CERT_KEY_FILE_TYPE "Flipper U2F Certificate Key File"
#define U2F_CERT_KEY_VERSION 1

#define U2F_DEVICE_KEY_FILE_TYPE "Flipper U2F Device Key File"
#define U2F_DEVICE_KEY_VERSION 1
#define U2F_DEVICE_KEY_MAC_DOMAIN "ZeroFIDO U2F device key file v1"

#define U2F_COUNTER_FILE_TYPE "Flipper U2F Counter File"
#define U2F_COUNTER_VERSION 2
#define U2F_CERT_MAX_SIZE 1024

#define U2F_COUNTER_CONTROL_VAL 0xAA5500FF
typedef struct {
    uint32_t counter;
    uint8_t random_salt[24];
    uint32_t control;
} FURI_PACKED U2fCounterData;

static const uint8_t u2f_generated_attestation_subject[] = {
    0x30, 0x49, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 'B',  'G',
    0x31, 0x11, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x08, 'Z',  'e',  'r',  'o',
    'F',  'I',  'D',  'O',  0x31, 0x27, 0x30, 0x25, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x1E,
    'Z',  'e',  'r',  'o',  'F',  'I',  'D',  'O',  ' ',  'L',  'o',  'c',  'a',  'l',  ' ',
    'U',  '2',  'F',  ' ',  'A',  't',  't',  'e',  's',  't',  'a',  't',  'i',  'o',  'n',
};

static const uint8_t u2f_generated_attestation_extensions[] = {
    0x30, 0x33, 0x30, 0x0C, 0x06, 0x03, 0x55, 0x1D, 0x13, 0x01, 0x01, 0xFF, 0x04, 0x02,
    0x30, 0x00, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04,
    0x03, 0x02, 0x07, 0x80, 0x30, 0x13, 0x06, 0x0B, 0x2B, 0x06, 0x01, 0x04, 0x01, 0x82,
    0xE5, 0x1C, 0x02, 0x01, 0x01, 0x04, 0x04, 0x03, 0x02, 0x05, 0x20,
};

static const ZfLocalAttestationProfile u2f_generated_attestation_profile = {
    .data_dir = U2F_DATA_FOLDER,
    .assets_dir = U2F_ASSETS_FOLDER,
    .cert_file = U2F_CERT_FILE,
    .cert_temp_file = U2F_CERT_FILE_TMP,
    .key_file = U2F_CERT_KEY_FILE,
    .key_temp_file = U2F_CERT_KEY_FILE_TMP,
    .key_file_type = U2F_CERT_KEY_FILE_TYPE,
    .subject_der = u2f_generated_attestation_subject,
    .subject_der_len = sizeof(u2f_generated_attestation_subject),
    .extensions_der = u2f_generated_attestation_extensions,
    .extensions_der_len = sizeof(u2f_generated_attestation_extensions),
    .identity = NULL,
    .identity_len = 0,
};

static uint32_t u2f_data_counter_high_water(uint32_t counter) {
    uint32_t available = UINT32_MAX - counter;

    if (available < ZF_COUNTER_RESERVATION_WINDOW) {
        return UINT32_MAX;
    }
    return counter + ZF_COUNTER_RESERVATION_WINDOW;
}

static void u2f_data_recover_atomic_files(Storage *storage) {
    if (!storage) {
        return;
    }
    zf_storage_recover_atomic_file(storage, U2F_CERT_FILE, U2F_CERT_FILE_TMP);
    zf_storage_recover_atomic_file(storage, U2F_CERT_KEY_FILE, U2F_CERT_KEY_FILE_TMP);
    zf_storage_recover_atomic_file(storage, U2F_KEY_FILE, U2F_KEY_FILE_TMP);
    zf_storage_recover_atomic_file(storage, U2F_CNT_FILE, U2F_CNT_FILE_TMP);
}

/* Shared file existence probe used to distinguish missing bootstrap files from parse failures. */
static bool u2f_data_file_exists(const char *path) {
    bool exists = false;
    Storage *storage = furi_record_open(RECORD_STORAGE);

    if (!storage) {
        return false;
    }
    u2f_data_recover_atomic_files(storage);
    exists = storage_file_exists(storage, path);
    furi_record_close(RECORD_STORAGE);
    return exists;
}

/* Creates the app and U2F asset directories before any atomic file publication. */
static bool u2f_data_ensure_directories(Storage *storage) {
    if (!storage) {
        return false;
    }
    if (!zf_storage_ensure_app_data_dir(storage)) {
        return false;
    }
    if (!zf_storage_ensure_dir(storage, U2F_DATA_FOLDER)) {
        return false;
    }
    if (!zf_storage_ensure_dir(storage, U2F_ASSETS_FOLDER)) {
        return false;
    }
    return true;
}

bool u2f_data_key_exists(void) {
    return u2f_data_file_exists(U2F_KEY_FILE);
}

bool u2f_data_cnt_exists(void) {
    return u2f_data_file_exists(U2F_CNT_FILE);
}

bool u2f_data_check(bool cert_only) {
    Storage *fs_api = furi_record_open(RECORD_STORAGE);

    if (!fs_api) {
        return false;
    }
    u2f_data_recover_atomic_files(fs_api);
    bool state = storage_file_exists(fs_api, U2F_CERT_FILE) &&
                 storage_file_exists(fs_api, U2F_CERT_KEY_FILE) &&
                 (cert_only || (storage_file_exists(fs_api, U2F_KEY_FILE) &&
                                storage_file_exists(fs_api, U2F_CNT_FILE)));
    furi_record_close(RECORD_STORAGE);

    return state;
}

static bool u2f_data_cert_public_key_load(uint8_t public_key[65]) {
    uint8_t *cert = malloc(U2F_CERT_MAX_SIZE);
    uint32_t cert_len = 0;
    bool ok = false;

    if (!cert) {
        return false;
    }

    cert_len = u2f_data_cert_load(cert, U2F_CERT_MAX_SIZE);
    if (cert_len > 0) {
        ok = zf_local_attestation_extract_cert_public_key(cert, cert_len, public_key);
    }

    zf_crypto_secure_zero(cert, U2F_CERT_MAX_SIZE);
    free(cert);
    return ok;
}

/* Loads the certificate and verifies it contains a parseable P-256 public key. */
bool u2f_data_cert_check(void) {
    uint8_t public_key[65];

    return u2f_data_cert_public_key_load(public_key);
}

/* Returns the stored DER certificate bytes used in U2F register responses. */
uint32_t u2f_data_cert_load(uint8_t *cert, size_t capacity) {
    furi_assert(cert);

    Storage *fs_api = furi_record_open(RECORD_STORAGE);
    uint32_t len_cur = 0;
    size_t cert_len = 0U;

    u2f_data_recover_atomic_files(fs_api);
    if (zf_storage_read_file(fs_api, U2F_CERT_FILE, cert, capacity, &cert_len)) {
        len_cur = cert_len;
    }

    furi_record_close(RECORD_STORAGE);

    return len_cur;
}

/* Encrypts imported/generated U2F attestation private keys under the device unique key. */
static bool u2f_data_cert_key_encrypt(uint8_t *cert_key) {
    furi_assert(cert_key);

    bool state = false;
    Storage *storage = NULL;

    FURI_LOG_I(TAG, "Encrypting user cert key");

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return false;
    }
    if (u2f_data_ensure_directories(storage)) {
        const ZfStorageEncryptedBlobWriteSpec spec = {
            .file_type = U2F_CERT_KEY_FILE_TYPE,
            .version = U2F_CERT_KEY_VERSION,
            .has_type = true,
            .type = U2F_CERT_USER,
            .key_slot = U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE,
            .plaintext = cert_key,
            .plaintext_len = 32U,
            .encrypted_len = 48U,
        };
        state = zf_storage_write_encrypted_blob_atomic(storage, U2F_CERT_KEY_FILE,
                                                       U2F_CERT_KEY_FILE_TMP, &spec);
    }
    furi_record_close(RECORD_STORAGE);
    return state;
}

/* Loads the encrypted user U2F attestation key. Legacy stock/plaintext key files are rejected. */
bool u2f_data_cert_key_load(uint8_t *cert_key) {
    furi_assert(cert_key);

    bool state = false;
    Storage *storage = NULL;
    const uint32_t accepted_versions[] = {U2F_CERT_KEY_VERSION};

    zf_crypto_secure_zero(cert_key, 32);

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return false;
    }

    const ZfStorageEncryptedBlobReadSpec spec = {
        .file_type = U2F_CERT_KEY_FILE_TYPE,
        .accepted_versions = accepted_versions,
        .accepted_version_count = 1U,
        .has_type = true,
        .expected_type = U2F_CERT_USER,
        .key_slot = U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE,
        .plaintext = cert_key,
        .plaintext_len = 32U,
        .encrypted_len = 48U,
    };
    state = zf_storage_read_encrypted_blob(storage, U2F_CERT_KEY_FILE, &spec);
    furi_record_close(RECORD_STORAGE);
    if (!state) {
        zf_crypto_secure_zero(cert_key, 32);
    }
    return state;
}

/* Derives the cert public key from the private key and compares it to the stored cert. */
bool u2f_data_cert_key_matches(const uint8_t *cert_key) {
    bool state = false;
    uint8_t *cert = NULL;
    uint32_t cert_len = 0;

    if (!cert_key) {
        return false;
    }

    cert = malloc(U2F_CERT_MAX_SIZE);
    if (!cert) {
        return false;
    }

    cert_len = u2f_data_cert_load(cert, U2F_CERT_MAX_SIZE);
    state = cert_len > 0U &&
            zf_local_attestation_private_key_matches_cert(cert_key, cert, cert_len, NULL, 0U);
    if (!state) {
        FURI_LOG_E(TAG, "Certificate/public key mismatch");
    }

    zf_crypto_secure_zero(cert, U2F_CERT_MAX_SIZE);
    free(cert);
    return state;
}

/* Imports explicit attestation assets, publishing the cert only if the key encrypts. */
bool u2f_data_bootstrap_attestation_assets(const uint8_t *cert, size_t cert_len,
                                           const uint8_t *cert_key, size_t cert_key_len) {
    bool ok = false;
    uint8_t cert_key_copy[32];
    Storage *storage = NULL;

    if (!cert || !cert_key || cert_len == 0 || cert_len > U2F_CERT_MAX_SIZE || cert_key_len != 32) {
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return false;
    }

    do {
        if (!u2f_data_ensure_directories(storage)) {
            break;
        }
        if (!zf_storage_write_file_atomic(storage, U2F_CERT_FILE, U2F_CERT_FILE_TMP, cert,
                                          cert_len)) {
            break;
        }
        memcpy(cert_key_copy, cert_key, sizeof(cert_key_copy));
        if (!u2f_data_cert_key_encrypt(cert_key_copy)) {
            zf_crypto_secure_zero(cert_key_copy, sizeof(cert_key_copy));
            zf_storage_remove_optional(storage, U2F_CERT_FILE);
            break;
        }
        zf_crypto_secure_zero(cert_key_copy, sizeof(cert_key_copy));
        ok = true;
    } while (false);

    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* Generates local U2F attestation assets through the shared local-attestation writer. */
bool u2f_data_generate_attestation_assets(void) {
    return zf_local_attestation_ensure_assets(&u2f_generated_attestation_profile);
}

/* MACs the encrypted device-key file using the plaintext key as the HMAC secret. */
static bool u2f_data_device_key_compute_mac(const uint8_t device_key[32], const uint8_t iv[16],
                                            const uint8_t key_encrypted[48], uint8_t mac[32]) {
    uint8_t material[64];
    bool ok = false;

    if (!device_key || !iv || !key_encrypted || !mac) {
        return false;
    }

    memcpy(material, iv, 16);
    memcpy(material + 16, key_encrypted, 48);
    ok = zf_crypto_hmac_sha256_parts(device_key, 32, (const uint8_t *)U2F_DEVICE_KEY_MAC_DOMAIN,
                                     sizeof(U2F_DEVICE_KEY_MAC_DOMAIN) - 1U, material,
                                     sizeof(material), mac);
    zf_crypto_secure_zero(material, sizeof(material));
    return ok;
}

static bool u2f_data_device_key_write_mac(const uint8_t *plaintext, size_t plaintext_len,
                                          const uint8_t iv[16], const uint8_t *encrypted,
                                          size_t encrypted_len, uint8_t *mac, size_t mac_capacity,
                                          size_t *mac_len, void *context) {
    UNUSED(context);
    if (plaintext_len != 32U || encrypted_len != 48U || mac_capacity < 32U) {
        return false;
    }
    if (!u2f_data_device_key_compute_mac(plaintext, iv, encrypted, mac)) {
        return false;
    }
    *mac_len = 32U;
    return true;
}

static bool u2f_data_device_key_verify_mac(const uint8_t *plaintext, size_t plaintext_len,
                                           const uint8_t iv[16], const uint8_t *encrypted,
                                           size_t encrypted_len, const uint8_t *mac, size_t mac_len,
                                           void *context) {
    uint8_t expected_mac[32];
    bool ok = false;

    UNUSED(context);
    if (plaintext_len != 32U || encrypted_len != 48U || !mac || mac_len != 32U) {
        return false;
    }
    ok = u2f_data_device_key_compute_mac(plaintext, iv, encrypted, expected_mac) &&
         zf_crypto_constant_time_equal(mac, expected_mac, sizeof(expected_mac));
    zf_crypto_secure_zero(expected_mac, sizeof(expected_mac));
    return ok;
}

/* Persists the U2F device key encrypted and authenticated under the unique key. */
static bool u2f_data_key_store_plaintext(const uint8_t key[32]) {
    bool state = false;
    Storage *storage = NULL;

    if (!key) {
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return false;
    }
    if (u2f_data_ensure_directories(storage)) {
        const ZfStorageEncryptedBlobWriteSpec spec = {
            .file_type = U2F_DEVICE_KEY_FILE_TYPE,
            .version = U2F_DEVICE_KEY_VERSION,
            .key_slot = U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE,
            .plaintext = key,
            .plaintext_len = 32U,
            .encrypted_len = 48U,
            .write_mac = u2f_data_device_key_write_mac,
        };
        state =
            zf_storage_write_encrypted_blob_atomic(storage, U2F_KEY_FILE, U2F_KEY_FILE_TMP, &spec);
    }
    furi_record_close(RECORD_STORAGE);
    return state;
}

/* Loads the U2F device key and verifies the file MAC before accepting it. */
bool u2f_data_key_load(uint8_t *device_key) {
    furi_assert(device_key);

    bool state = false;
    Storage *storage = furi_record_open(RECORD_STORAGE);
    const uint32_t accepted_versions[] = {U2F_DEVICE_KEY_VERSION};

    if (!storage) {
        return false;
    }

    const ZfStorageEncryptedBlobReadSpec spec = {
        .file_type = U2F_DEVICE_KEY_FILE_TYPE,
        .accepted_versions = accepted_versions,
        .accepted_version_count = 1U,
        .key_slot = U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE,
        .plaintext = device_key,
        .plaintext_len = 32U,
        .encrypted_len = 48U,
        .has_mac = true,
        .mac_len = 32U,
        .verify_mac = u2f_data_device_key_verify_mac,
    };
    state = zf_storage_read_encrypted_blob(storage, U2F_KEY_FILE, &spec);
    furi_record_close(RECORD_STORAGE);
    return state;
}

/* Generates the device secret from which U2F credential private keys are derived. */
bool u2f_data_key_generate(uint8_t *device_key) {
    furi_assert(device_key);

    bool state = false;
    uint8_t key[32];

    if (!furi_hal_crypto_enclave_ensure_key(U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE)) {
        FURI_LOG_E(TAG, "Unable to ensure encryption key");
        return false;
    }

    // Generate random device secret used to derive U2F credential keys.
    furi_hal_random_fill_buf(key, 32);

    state = u2f_data_key_store_plaintext(key);
    if (state) {
        memcpy(device_key, key, 32);
    }

    zf_crypto_secure_zero(key, sizeof(key));
    return state;
}

/* Reads the encrypted U2F counter. Legacy counter versions are rejected. */
bool u2f_data_cnt_read(uint32_t *cnt_val) {
    furi_assert(cnt_val);

    bool state = false;
    U2fCounterData cnt = {0};
    Storage *storage = furi_record_open(RECORD_STORAGE);
    const uint32_t accepted_versions[] = {U2F_COUNTER_VERSION};

    if (!storage) {
        return false;
    }

    const ZfStorageEncryptedBlobReadSpec spec = {
        .file_type = U2F_COUNTER_FILE_TYPE,
        .accepted_versions = accepted_versions,
        .accepted_version_count = 1U,
        .key_slot = U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE,
        .plaintext = (uint8_t *)&cnt,
        .plaintext_len = sizeof(cnt),
        .encrypted_len = 48U,
    };
    if (zf_storage_read_encrypted_blob(storage, U2F_CNT_FILE, &spec) &&
        cnt.control == U2F_COUNTER_CONTROL_VAL) {
        *cnt_val = cnt.counter;
        state = true;
    }
    furi_record_close(RECORD_STORAGE);
    zf_crypto_secure_zero(&cnt, sizeof(cnt));
    return state;
}

bool u2f_data_cnt_write(uint32_t cnt_val) {
    bool state = false;
    U2fCounterData cnt;
    Storage *storage = NULL;

    // Generate random IV and key
    furi_hal_random_fill_buf(cnt.random_salt, 24);
    cnt.control = U2F_COUNTER_CONTROL_VAL;
    cnt.counter = cnt_val;

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        zf_crypto_secure_zero(&cnt, sizeof(cnt));
        return false;
    }
    if (u2f_data_ensure_directories(storage)) {
        const ZfStorageEncryptedBlobWriteSpec spec = {
            .file_type = U2F_COUNTER_FILE_TYPE,
            .version = U2F_COUNTER_VERSION,
            .key_slot = U2F_DATA_FILE_ENCRYPTION_KEY_SLOT_UNIQUE,
            .plaintext = (const uint8_t *)&cnt,
            .plaintext_len = sizeof(cnt),
            .encrypted_len = 48U,
        };
        state =
            zf_storage_write_encrypted_blob_atomic(storage, U2F_CNT_FILE, U2F_CNT_FILE_TMP, &spec);
    }
    furi_record_close(RECORD_STORAGE);
    zf_crypto_secure_zero(&cnt, sizeof(cnt));
    return state;
}

/*
 * Reserves a durable future counter before an authentication response commits
 * its in-memory value. This mirrors the CTAP credential counter high-water
 * pattern and protects against rollback after interrupted writes.
 */
bool u2f_data_cnt_reserve(uint32_t cnt_val, uint32_t *reserved_cnt) {
    uint32_t high_water = u2f_data_counter_high_water(cnt_val);

    if (!u2f_data_cnt_write(high_water)) {
        return false;
    }
    if (reserved_cnt) {
        *reserved_cnt = high_water;
    }
    return true;
}

bool u2f_data_wipe(Storage *storage) {
    if (!storage) {
        return false;
    }

    return zf_storage_remove_atomic_file(storage, U2F_KEY_FILE, U2F_KEY_FILE_TMP) &&
           zf_storage_remove_atomic_file(storage, U2F_CNT_FILE, U2F_CNT_FILE_TMP);
}
