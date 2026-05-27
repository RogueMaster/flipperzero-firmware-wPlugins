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

#include "state_store.h"
#include "internal.h"

#include <furi_hal.h>
#include <furi_hal_random.h>
#include <string.h>

#include "../../zerofido_crypto.h"
#include "../../zerofido_storage.h"

/* Writes an invalid-version record when retry state persistence must fail closed. */
static bool zf_pin_invalidate_persisted_state(Storage *storage) {
    const ZfPinFileRecord invalid_record = {
        .base =
            {
                .magic = ZF_PIN_FILE_MAGIC,
                .version = UINT8_MAX,
            },
    };
    File *file = storage_file_alloc(storage);
    bool ok = false;

    if (!file) {
        return false;
    }
    if (storage_file_open(file, ZF_PIN_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, &invalid_record, sizeof(invalid_record)) ==
             sizeof(invalid_record);
        storage_file_close(file);
    }
    storage_file_free(file);
    return ok;
}

/* Digest binds retry counters and auth-blocked flag to the encrypted PIN hash. */
static void zf_pin_compute_retry_state_digest(const uint8_t pin_hash[ZF_PIN_HASH_LEN],
                                              uint8_t pin_retries,
                                              uint8_t pin_consecutive_mismatches, uint8_t flags,
                                              uint8_t digest[32]) {
    uint8_t material[ZF_PIN_HASH_LEN + 3];

    memcpy(material, pin_hash, ZF_PIN_HASH_LEN);
    material[ZF_PIN_HASH_LEN + 0] = pin_retries;
    material[ZF_PIN_HASH_LEN + 1] = pin_consecutive_mismatches;
    material[ZF_PIN_HASH_LEN + 2] = flags;
    zf_crypto_sha256(material, sizeof(material), digest);
    zf_crypto_secure_zero(material, sizeof(material));
}

/* Encrypts retry counters separately so partial tampering blocks PIN use. */
static bool zf_pin_seal_retry_state(const uint8_t pin_hash[ZF_PIN_HASH_LEN], uint8_t pin_retries,
                                    uint8_t pin_consecutive_mismatches, uint8_t flags,
                                    uint8_t iv[ZF_WRAP_IV_LEN],
                                    uint8_t sealed[ZF_PIN_RETRY_SEAL_SIZE]) {
    ZfPinRetrySeal plain = {
        .magic = ZF_PIN_RETRY_SEAL_MAGIC,
        .pin_retries = pin_retries,
        .pin_consecutive_mismatches = pin_consecutive_mismatches,
        .flags = flags,
    };
    uint8_t digest[32];

    zf_pin_compute_retry_state_digest(pin_hash, pin_retries, pin_consecutive_mismatches, flags,
                                      digest);
    memcpy(plain.digest, digest, sizeof(plain.digest));
    furi_hal_random_fill_buf(iv, ZF_WRAP_IV_LEN);
    if (!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
        zf_crypto_secure_zero(&plain, sizeof(plain));
        zf_crypto_secure_zero(digest, sizeof(digest));
        return false;
    }

    bool ok = furi_hal_crypto_encrypt((const uint8_t *)&plain, sealed, sizeof(plain));
    furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
    zf_crypto_secure_zero(&plain, sizeof(plain));
    zf_crypto_secure_zero(digest, sizeof(digest));
    return ok;
}

/* Verifies the retry seal before accepting persisted PIN state. */
static bool zf_pin_verify_retry_state(const uint8_t pin_hash[ZF_PIN_HASH_LEN], uint8_t pin_retries,
                                      uint8_t pin_consecutive_mismatches, uint8_t flags,
                                      const uint8_t iv[ZF_WRAP_IV_LEN],
                                      const uint8_t sealed[ZF_PIN_RETRY_SEAL_SIZE]) {
    ZfPinRetrySeal plain = {0};
    uint8_t digest[32];

    if (!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
        return false;
    }

    bool ok = furi_hal_crypto_decrypt(sealed, (uint8_t *)&plain, sizeof(plain));
    furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
    if (!ok || plain.magic != ZF_PIN_RETRY_SEAL_MAGIC || plain.pin_retries != pin_retries ||
        plain.pin_consecutive_mismatches != pin_consecutive_mismatches || plain.flags != flags ||
        plain.reserved != 0U) {
        zf_crypto_secure_zero(&plain, sizeof(plain));
        return false;
    }

    zf_pin_compute_retry_state_digest(pin_hash, pin_retries, pin_consecutive_mismatches, flags,
                                      digest);
    ok = zf_crypto_constant_time_equal(plain.digest, digest, sizeof(plain.digest));
    zf_crypto_secure_zero(&plain, sizeof(plain));
    zf_crypto_secure_zero(digest, sizeof(digest));
    return ok;
}

/* Recovers or removes files left by interrupted PIN persistence. */
void zf_pin_state_store_cleanup_temp(Storage *storage) {
    zf_storage_recover_atomic_file(storage, ZF_PIN_FILE_PATH, ZF_PIN_FILE_TEMP_PATH);
}

/* Loads and authenticates persisted PIN hash/retry state. */
ZfPinLoadStatus zf_pin_state_store_load(Storage *storage, uint8_t pin_hash[ZF_PIN_HASH_LEN],
                                        uint8_t *pin_retries, uint8_t *pin_consecutive_mismatches,
                                        bool *pin_auth_blocked) {
    uint8_t raw[sizeof(ZfPinFileRecord)];
    const ZfPinFileRecord *record = NULL;
    size_t raw_size = 0;
    bool ok = false;

    if (!storage) {
        return ZfPinLoadInvalid;
    }
    if (!zf_storage_read_file(storage, ZF_PIN_FILE_PATH, raw, sizeof(raw), &raw_size)) {
        return storage_file_exists(storage, ZF_PIN_FILE_PATH) ? ZfPinLoadInvalid : ZfPinLoadMissing;
    }
    if (raw_size != sizeof(ZfPinFileRecord)) {
        return ZfPinLoadInvalid;
    }
    record = (const ZfPinFileRecord *)raw;
    if (record->base.magic != ZF_PIN_FILE_MAGIC) {
        return ZfPinLoadInvalid;
    }
    if (record->base.version != ZF_PIN_FILE_VERSION ||
        record->base.pin_retries > ZF_PIN_RETRIES_MAX ||
        record->base.pin_consecutive_mismatches > 3 ||
        (record->base.flags & ~ZF_PIN_FILE_FLAG_AUTH_BLOCKED) != 0) {
        return ZfPinLoadInvalid;
    }
    *pin_retries = record->base.pin_retries;
    *pin_consecutive_mismatches = record->base.pin_consecutive_mismatches;
    *pin_auth_blocked = (record->base.flags & ZF_PIN_FILE_FLAG_AUTH_BLOCKED) != 0;

    if (!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT,
                                          record->base.iv)) {
        return ZfPinLoadInvalid;
    }
    ok = furi_hal_crypto_decrypt(record->base.encrypted_pin_hash, pin_hash,
                                 sizeof(record->base.encrypted_pin_hash));
    furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
    if (!ok) {
        return ZfPinLoadInvalid;
    }
    if (!zf_pin_verify_retry_state(pin_hash, *pin_retries, *pin_consecutive_mismatches,
                                   record->base.flags, record->retry_seal_iv,
                                   record->encrypted_retry_seal)) {
        return ZfPinLoadInvalid;
    }

    return ZfPinLoadOk;
}

/* Publishes a fresh encrypted PIN state file through temp-file rename. */
bool zf_pin_state_store_persist(Storage *storage, const ZfClientPinState *state) {
    ZfPinFileRecord record = {
        .base =
            {
                .magic = ZF_PIN_FILE_MAGIC,
                .version = ZF_PIN_FILE_VERSION,
                .pin_retries = state->pin_retries,
                .pin_consecutive_mismatches = state->pin_consecutive_mismatches,
                .flags = state->pin_auth_blocked ? ZF_PIN_FILE_FLAG_AUTH_BLOCKED : 0U,
            },
    };
    bool ok = false;

    if (!state->pin_set) {
        return true;
    }
    if (!zf_storage_ensure_app_data_dir(storage)) {
        return false;
    }

    furi_hal_random_fill_buf(record.base.iv, sizeof(record.base.iv));
    if (!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT,
                                          record.base.iv)) {
        return false;
    }
    ok = furi_hal_crypto_encrypt(state->pin_hash, record.base.encrypted_pin_hash,
                                 sizeof(record.base.encrypted_pin_hash));
    furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
    if (!ok) {
        return false;
    }
    if (!zf_pin_seal_retry_state(state->pin_hash, state->pin_retries,
                                 state->pin_consecutive_mismatches, record.base.flags,
                                 record.retry_seal_iv, record.encrypted_retry_seal)) {
        return false;
    }

    return zf_storage_write_file_atomic(storage, ZF_PIN_FILE_PATH, ZF_PIN_FILE_TEMP_PATH,
                                        (const uint8_t *)&record, sizeof(record));
}

bool zf_pin_state_store_fail_closed(Storage *storage, const ZfClientPinState *state) {
    if (!state->pin_set) {
        return true;
    }

    if (zf_pin_invalidate_persisted_state(storage)) {
        return true;
    }

    return zf_storage_remove_atomic_file(storage, ZF_PIN_FILE_PATH, ZF_PIN_FILE_TEMP_PATH);
}

bool zf_pin_state_store_clear(Storage *storage) {
    return zf_storage_remove_atomic_file(storage, ZF_PIN_FILE_PATH, ZF_PIN_FILE_TEMP_PATH);
}
