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

#include "record_format.h"

#include <furi.h>
#include <furi_hal.h>
#include <string.h>

#include "../ctap/extensions/cred_protect.h"
#include "../zerofido_cbor.h"
#include "../zerofido_crypto.h"
#include "../zerofido_storage.h"
#include "../zerofido_store.h"
#include "internal.h"
#include "record_format_internal.h"

/*
 * Credential records are stored as CBOR maps named by the lowercase hex
 * credential ID. The file name is part of validation: a record whose decoded
 * credential_id does not match its file name is rejected.
 *
 * Each credential also has a companion .counter floor file. It keeps
 * sign_count rollback detectable across record-file rewrites and interrupted
 * updates without embedding a second encrypted seal in the CBOR record.
 */

#define ZF_STORE_VERSION ZF_STORE_FORMAT_VERSION
#define ZF_COUNTER_FLOOR_MAGIC 0x53434632UL
#define ZF_COUNTER_FLOOR_VERSION 1U

enum {
    ZfRecordKeyVersion = 1,
    ZfRecordKeyCredentialId = 2,
    ZfRecordKeyRpId = 3,
    ZfRecordKeyUserId = 4,
    ZfRecordKeyUserName = 5,
    ZfRecordKeyDisplayName = 6,
    ZfRecordKeyPublicX = 7,
    ZfRecordKeyPublicY = 8,
    ZfRecordKeyPrivateWrapped = 9,
    ZfRecordKeyPrivateIv = 10,
    ZfRecordKeySignCount = 11,
    ZfRecordKeyCreatedAt = 12,
    ZfRecordKeyResidentKey = 13,
    ZfRecordKeyCredProtect = 14,
    ZfRecordKeyHmacSecretWrapped = 15,
    ZfRecordKeyHmacSecretIv = 16,
    ZfRecordKeyCounterFloor = 17,
};

#define ZF_RECORD_HMAC_SECRET_STORAGE_LEN (ZF_HMAC_SECRET_LEN * 2U)

typedef struct {
    uint32_t magic;
    uint32_t sign_count;
    uint8_t binding[24];
} ZfCounterFloorSeal;

typedef struct {
    uint32_t version;
    uint8_t iv[ZF_WRAP_IV_LEN];
    uint8_t sealed[sizeof(ZfCounterFloorSeal)];
} ZfCounterFloorFile;

static void
zf_record_compute_counter_binding_fields(const uint8_t credential_id[ZF_CREDENTIAL_ID_LEN],
                                         uint32_t created_at, uint8_t binding[24]) {
    uint8_t material[ZF_CREDENTIAL_ID_LEN + sizeof(created_at)];
    uint8_t full_digest[32];
    size_t offset = 0;

    memcpy(material + offset, credential_id, ZF_CREDENTIAL_ID_LEN);
    offset += ZF_CREDENTIAL_ID_LEN;
    memcpy(material + offset, &created_at, sizeof(created_at));
    offset += sizeof(created_at);
    zf_crypto_sha256(material, offset, full_digest);
    memcpy(binding, full_digest, 24);
    zf_crypto_secure_zero(full_digest, sizeof(full_digest));
}

static uint32_t zf_counter_reserved_high_water(uint32_t sign_count) {
    uint32_t available = UINT32_MAX - sign_count;

    if (available < ZF_COUNTER_RESERVATION_WINDOW) {
        return UINT32_MAX;
    }
    return sign_count + ZF_COUNTER_RESERVATION_WINDOW;
}

static bool zf_counter_floor_encode_fields(const uint8_t credential_id[ZF_CREDENTIAL_ID_LEN],
                                           uint32_t created_at, uint32_t sign_count,
                                           ZfCounterFloorFile *out_file) {
    ZfCounterFloorSeal plain = {
        .magic = ZF_COUNTER_FLOOR_MAGIC,
        .sign_count = sign_count,
    };

    memset(out_file, 0, sizeof(*out_file));
    out_file->version = ZF_COUNTER_FLOOR_VERSION;
    zf_record_compute_counter_binding_fields(credential_id, created_at, plain.binding);
    furi_hal_random_fill_buf(out_file->iv, sizeof(out_file->iv));
    if (!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, out_file->iv)) {
        zf_crypto_secure_zero(&plain, sizeof(plain));
        zf_crypto_secure_zero(out_file, sizeof(*out_file));
        return false;
    }

    bool ok = furi_hal_crypto_encrypt((const uint8_t *)&plain, out_file->sealed, sizeof(plain));
    furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
    zf_crypto_secure_zero(&plain, sizeof(plain));
    if (!ok) {
        zf_crypto_secure_zero(out_file, sizeof(*out_file));
    }
    return ok;
}

static bool zf_counter_floor_decode_fields(const uint8_t credential_id[ZF_CREDENTIAL_ID_LEN],
                                           uint32_t created_at,
                                           const ZfCounterFloorFile *counter_file,
                                           uint32_t *stored_sign_count) {
    ZfCounterFloorSeal plain = {0};
    uint8_t expected_binding[24];

    if (!credential_id || !counter_file || !stored_sign_count ||
        counter_file->version != ZF_COUNTER_FLOOR_VERSION) {
        return false;
    }
    if (!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT,
                                          counter_file->iv)) {
        return false;
    }

    bool ok = furi_hal_crypto_decrypt(counter_file->sealed, (uint8_t *)&plain, sizeof(plain));
    furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
    if (!ok || plain.magic != ZF_COUNTER_FLOOR_MAGIC) {
        zf_crypto_secure_zero(&plain, sizeof(plain));
        return false;
    }

    zf_record_compute_counter_binding_fields(credential_id, created_at, expected_binding);
    ok = zf_crypto_constant_time_equal(plain.binding, expected_binding, sizeof(plain.binding));
    if (ok) {
        *stored_sign_count = plain.sign_count;
    }
    zf_crypto_secure_zero(expected_binding, sizeof(expected_binding));
    zf_crypto_secure_zero(&plain, sizeof(plain));
    return ok;
}

static bool
zf_store_counter_floor_write_value_fields(Storage *storage, const char *file_name,
                                          const uint8_t credential_id[ZF_CREDENTIAL_ID_LEN],
                                          uint32_t created_at, uint32_t sign_count) {
    ZfCounterFloorFile counter_file;
    char path[128];
    char temp_path[128];
    bool ok = false;

    if (!storage || !file_name || !credential_id) {
        return false;
    }
    if (!zf_counter_floor_encode_fields(credential_id, created_at, sign_count, &counter_file)) {
        return false;
    }

    zf_store_build_counter_floor_path(file_name, path, sizeof(path));
    zf_store_build_counter_floor_temp_path(file_name, temp_path, sizeof(temp_path));
    ok = zf_storage_write_file_atomic(storage, path, temp_path, (const uint8_t *)&counter_file,
                                      sizeof(counter_file));
    zf_crypto_secure_zero(&counter_file, sizeof(counter_file));
    return ok;
}

static bool zf_store_counter_floor_write_value(Storage *storage, const ZfCredentialRecord *record,
                                               uint32_t sign_count) {
    if (!record) {
        return false;
    }
    return zf_store_counter_floor_write_value_fields(
        storage, record->file_name, record->credential_id, record->created_at, sign_count);
}

/*
 * Validates the per-credential counter floor. A companion .counter file wins
 * when present because it is the post-assertion high-water mark. New records
 * can bootstrap from their embedded floor, while legacy records without an
 * embedded floor get a companion file on first validated load.
 */
static bool zf_store_counter_floor_validate_fields(
    Storage *storage, const char *file_name, const uint8_t credential_id[ZF_CREDENTIAL_ID_LEN],
    uint32_t created_at, uint32_t *sign_count, const ZfCounterFloorFile *embedded_counter_floor,
    bool has_embedded_counter_floor, uint32_t *out_high_water) {
    char path[128];
    uint8_t buffer[sizeof(ZfCounterFloorFile)] = {0};
    size_t size = 0;
    uint32_t stored_sign_count = 0;

    if (out_high_water) {
        *out_high_water = sign_count ? *sign_count : 0;
    }

    if (!storage || !file_name || !credential_id || !sign_count) {
        return false;
    }

    zf_store_build_counter_floor_path(file_name, path, sizeof(path));
    if (!storage_file_exists(storage, path)) {
        if (has_embedded_counter_floor) {
            if (!zf_counter_floor_decode_fields(credential_id, created_at, embedded_counter_floor,
                                                &stored_sign_count) ||
                stored_sign_count != *sign_count) {
                return false;
            }
            if (out_high_water) {
                *out_high_water = stored_sign_count;
            }
            return true;
        }
        if (!zf_store_counter_floor_write_value_fields(storage, file_name, credential_id,
                                                       created_at, *sign_count)) {
            return false;
        }
        if (out_high_water) {
            *out_high_water = *sign_count;
        }
        return true;
    }

    if (!zf_storage_read_file(storage, path, buffer, sizeof(buffer), &size) ||
        size != sizeof(ZfCounterFloorFile)) {
        return false;
    }

    if (!zf_counter_floor_decode_fields(credential_id, created_at,
                                        (const ZfCounterFloorFile *)buffer, &stored_sign_count)) {
        return false;
    }
    if (stored_sign_count > *sign_count) {
        *sign_count = stored_sign_count;
    } else if (stored_sign_count < *sign_count) {
        if (!zf_store_counter_floor_write_value_fields(storage, file_name, credential_id,
                                                       created_at, *sign_count)) {
            return false;
        }
        stored_sign_count = *sign_count;
    }
    if (out_high_water) {
        *out_high_water = stored_sign_count;
    }
    return true;
}

static bool zf_store_counter_floor_validate(Storage *storage, ZfCredentialRecord *record,
                                            const ZfCounterFloorFile *embedded_counter_floor,
                                            bool has_embedded_counter_floor,
                                            uint32_t *out_high_water) {
    if (!record) {
        return false;
    }
    return zf_store_counter_floor_validate_fields(
        storage, record->file_name, record->credential_id, record->created_at, &record->sign_count,
        embedded_counter_floor, has_embedded_counter_floor, out_high_water);
}

static bool zf_store_counter_floor_validate_index(Storage *storage, const char *file_name,
                                                  ZfCredentialIndexEntry *entry,
                                                  const ZfCounterFloorFile *embedded_counter_floor,
                                                  bool has_embedded_counter_floor,
                                                  uint32_t *out_high_water) {
    if (!entry) {
        return false;
    }
    return zf_store_counter_floor_validate_fields(
        storage, file_name, entry->credential_id, entry->created_at, &entry->sign_count,
        embedded_counter_floor, has_embedded_counter_floor, out_high_water);
}

static bool zf_record_wrap_hmac_secret(const ZfCredentialRecord *record,
                                       uint8_t wrapped[ZF_RECORD_HMAC_SECRET_STORAGE_LEN],
                                       uint8_t iv[ZF_WRAP_IV_LEN]) {
    uint8_t plain[ZF_RECORD_HMAC_SECRET_STORAGE_LEN];
    bool ok = false;

    if (!record || !wrapped || !iv) {
        return false;
    }

    memcpy(plain, record->hmac_secret_without_uv, ZF_HMAC_SECRET_LEN);
    memcpy(plain + ZF_HMAC_SECRET_LEN, record->hmac_secret_with_uv, ZF_HMAC_SECRET_LEN);
    furi_hal_random_fill_buf(iv, ZF_WRAP_IV_LEN);

    if (furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
        ok = furi_hal_crypto_encrypt(plain, wrapped, sizeof(plain));
        furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
    }

    zf_crypto_secure_zero(plain, sizeof(plain));
    if (!ok) {
        zf_crypto_secure_zero(wrapped, ZF_RECORD_HMAC_SECRET_STORAGE_LEN);
        zf_crypto_secure_zero(iv, ZF_WRAP_IV_LEN);
    }
    return ok;
}

static bool zf_record_unwrap_hmac_secret(ZfCredentialRecord *record,
                                         const uint8_t wrapped[ZF_RECORD_HMAC_SECRET_STORAGE_LEN],
                                         const uint8_t iv[ZF_WRAP_IV_LEN]) {
    uint8_t plain[ZF_RECORD_HMAC_SECRET_STORAGE_LEN];
    bool ok = false;

    if (!record || !wrapped || !iv) {
        return false;
    }

    if (furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
        ok = furi_hal_crypto_decrypt(wrapped, plain, sizeof(plain));
        furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
    }
    if (ok) {
        memcpy(record->hmac_secret_without_uv, plain, ZF_HMAC_SECRET_LEN);
        memcpy(record->hmac_secret_with_uv, plain + ZF_HMAC_SECRET_LEN, ZF_HMAC_SECRET_LEN);
    }

    zf_crypto_secure_zero(plain, sizeof(plain));
    return ok;
}

bool zf_store_record_format_reserve_counter_with_buffer(Storage *storage,
                                                        const ZfCredentialRecord *record,
                                                        uint8_t *buffer, size_t buffer_size,
                                                        uint32_t *out_high_water) {
    ZfCredentialRecord reserved_record;
    uint32_t high_water = 0;
    bool ok = false;

    if (!record || !buffer || buffer_size < ZF_STORE_RECORD_MAX_SIZE) {
        return false;
    }

    high_water = zf_counter_reserved_high_water(record->sign_count);
    if (!zf_store_counter_floor_write_value(storage, record, high_water)) {
        return false;
    }
    reserved_record = *record;
    reserved_record.sign_count = high_water;
    ok = zf_store_record_format_write_record_with_buffer(storage, &reserved_record, buffer,
                                                         buffer_size);
    zf_crypto_secure_zero(&reserved_record, sizeof(reserved_record));
    if (!ok) {
        return false;
    }
    if (out_high_water) {
        *out_high_water = high_water;
    }
    return true;
}

bool zf_store_record_format_reserve_counter(Storage *storage, const ZfCredentialRecord *record,
                                            uint32_t *out_high_water) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    bool ok = zf_store_record_format_reserve_counter_with_buffer(storage, record, buffer,
                                                                 sizeof(buffer), out_high_water);

    zf_crypto_secure_zero(buffer, sizeof(buffer));
    return ok;
}

bool zf_store_record_format_encode(const ZfCredentialRecord *record, uint8_t *out,
                                   size_t *out_size) {
    ZfCborEncoder enc;
    ZfCounterFloorFile counter_floor;
    uint8_t hmac_secret_wrapped[ZF_RECORD_HMAC_SECRET_STORAGE_LEN] = {0};
    uint8_t hmac_secret_iv[ZF_WRAP_IV_LEN] = {0};
    uint8_t effective_cred_protect = ZF_CRED_PROTECT_UV_OPTIONAL;

    if (!record || !out || !out_size ||
        !zf_cbor_encoder_init(&enc, out, ZF_STORE_RECORD_MAX_SIZE)) {
        return false;
    }
    effective_cred_protect = zf_ctap_cred_protect_effective(record->cred_protect);

    if (!zf_counter_floor_encode_fields(record->credential_id, record->created_at,
                                        record->sign_count, &counter_floor)) {
        return false;
    }

    size_t pairs = 15;
    if (record->hmac_secret) {
        pairs += 2;
        if (!zf_record_wrap_hmac_secret(record, hmac_secret_wrapped, hmac_secret_iv)) {
            zf_crypto_secure_zero(&counter_floor, sizeof(counter_floor));
            return false;
        }
    }

    bool ok =
        zf_cbor_encode_map(&enc, pairs) && zf_cbor_encode_uint(&enc, ZfRecordKeyVersion) &&
        zf_cbor_encode_uint(&enc, ZF_STORE_VERSION) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyCredentialId) &&
        zf_cbor_encode_bytes(&enc, record->credential_id, record->credential_id_len) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyRpId) && zf_cbor_encode_text(&enc, record->rp_id) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyUserId) &&
        zf_cbor_encode_bytes(&enc, record->user_id, record->user_id_len) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyUserName) &&
        zf_cbor_encode_text(&enc, record->user_name) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyDisplayName) &&
        zf_cbor_encode_text(&enc, record->user_display_name) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyPublicX) &&
        zf_cbor_encode_bytes(&enc, record->public_x, sizeof(record->public_x)) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyPublicY) &&
        zf_cbor_encode_bytes(&enc, record->public_y, sizeof(record->public_y)) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyPrivateWrapped) &&
        zf_cbor_encode_bytes(&enc, record->private_wrapped, sizeof(record->private_wrapped)) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyPrivateIv) &&
        zf_cbor_encode_bytes(&enc, record->private_iv, sizeof(record->private_iv)) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeySignCount) &&
        zf_cbor_encode_uint(&enc, record->sign_count) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyCreatedAt) &&
        zf_cbor_encode_uint(&enc, record->created_at) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyResidentKey) &&
        zf_cbor_encode_bool(&enc, record->resident_key) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyCredProtect) &&
        zf_cbor_encode_uint(&enc, effective_cred_protect) &&
        zf_cbor_encode_uint(&enc, ZfRecordKeyCounterFloor) &&
        zf_cbor_encode_bytes(&enc, (const uint8_t *)&counter_floor, sizeof(counter_floor));

    if (ok && record->hmac_secret) {
        ok = zf_cbor_encode_uint(&enc, ZfRecordKeyHmacSecretWrapped) &&
             zf_cbor_encode_bytes(&enc, hmac_secret_wrapped, sizeof(hmac_secret_wrapped)) &&
             zf_cbor_encode_uint(&enc, ZfRecordKeyHmacSecretIv) &&
             zf_cbor_encode_bytes(&enc, hmac_secret_iv, sizeof(hmac_secret_iv));
    }

    if (!ok) {
        zf_crypto_secure_zero(&counter_floor, sizeof(counter_floor));
        zf_crypto_secure_zero(hmac_secret_wrapped, sizeof(hmac_secret_wrapped));
        zf_crypto_secure_zero(hmac_secret_iv, sizeof(hmac_secret_iv));
        return false;
    }

    *out_size = zf_cbor_encoder_size(&enc);
    zf_crypto_secure_zero(&counter_floor, sizeof(counter_floor));
    zf_crypto_secure_zero(hmac_secret_wrapped, sizeof(hmac_secret_wrapped));
    zf_crypto_secure_zero(hmac_secret_iv, sizeof(hmac_secret_iv));
    return true;
}

static bool zf_copy_text_field(char *out, size_t out_size, const uint8_t *data, size_t size) {
    if (size >= out_size || memchr(data, '\0', size) != NULL) {
        return false;
    }

    memcpy(out, data, size);
    out[size] = '\0';
    return true;
}

void zf_store_record_format_hex_encode(const uint8_t *data, size_t size, char *out) {
    static const char *hex = "0123456789abcdef";

    for (size_t i = 0; i < size; ++i) {
        out[i * 2] = hex[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[data[i] & 0x0F];
    }
    out[size * 2] = '\0';
}

static bool zf_record_id_matches_file_name(const uint8_t *credential_id, size_t credential_id_len,
                                           const char *file_name) {
    char expected_name[(ZF_CREDENTIAL_ID_LEN * 2) + 1];

    if (!credential_id || !file_name || credential_id_len != ZF_CREDENTIAL_ID_LEN) {
        return false;
    }

    zf_store_record_format_hex_encode(credential_id, credential_id_len, expected_name);
    return strcmp(expected_name, file_name) == 0;
}

static bool zf_record_decode_text(char *out, size_t out_size, ZfCborCursor *cursor) {
    const uint8_t *ptr = NULL;
    size_t size = 0;

    return zf_cbor_read_text_ptr(cursor, &ptr, &size) &&
           zf_copy_text_field(out, out_size, ptr, size);
}

static bool zf_record_decode_bytes(uint8_t *target, size_t expected_size, ZfCborCursor *cursor) {
    const uint8_t *ptr = NULL;
    size_t size = 0;

    if (!zf_cbor_read_bytes_ptr(cursor, &ptr, &size) || size != expected_size) {
        return false;
    }

    memcpy(target, ptr, size);
    return true;
}

static bool zf_record_decode_version(ZfCborCursor *cursor, uint32_t *version) {
    uint64_t raw = 0;
    if (!zf_cbor_read_uint(cursor, &raw)) {
        return false;
    }
    if (raw > UINT32_MAX) {
        return false;
    }

    *version = (uint32_t)raw;
    return true;
}

static bool zf_record_decode_user_id(ZfCborCursor *cursor, ZfCredentialRecord *record) {
    const uint8_t *ptr = NULL;
    size_t size = 0;

    if (!zf_cbor_read_bytes_ptr(cursor, &ptr, &size) || size > ZF_MAX_USER_ID_LEN) {
        return false;
    }

    memcpy(record->user_id, ptr, size);
    record->user_id_len = size;
    return true;
}

static bool zf_record_decode_counter(ZfCborCursor *cursor, uint32_t *value) {
    uint64_t raw = 0;
    if (!zf_cbor_read_uint(cursor, &raw)) {
        return false;
    }
    if (raw > UINT32_MAX) {
        return false;
    }

    *value = (uint32_t)raw;
    return true;
}

static bool zf_record_decode_field(ZfCborCursor *cursor, uint64_t key, ZfCredentialRecord *record,
                                   uint32_t *version) {
    switch (key) {
    case ZfRecordKeyVersion:
        return zf_record_decode_version(cursor, version);
    case ZfRecordKeyCredentialId:
        return zf_record_decode_bytes(record->credential_id, ZF_CREDENTIAL_ID_LEN, cursor);
    case ZfRecordKeyRpId:
        return zf_record_decode_text(record->rp_id, sizeof(record->rp_id), cursor);
    case ZfRecordKeyUserId:
        return zf_record_decode_user_id(cursor, record);
    case ZfRecordKeyUserName:
        return zf_record_decode_text(record->user_name, sizeof(record->user_name), cursor);
    case ZfRecordKeyDisplayName:
        return zf_record_decode_text(record->user_display_name, sizeof(record->user_display_name),
                                     cursor);
    case ZfRecordKeyPublicX:
        return zf_record_decode_bytes(record->public_x, sizeof(record->public_x), cursor);
    case ZfRecordKeyPublicY:
        return zf_record_decode_bytes(record->public_y, sizeof(record->public_y), cursor);
    case ZfRecordKeyPrivateWrapped:
        return zf_record_decode_bytes(record->private_wrapped, sizeof(record->private_wrapped),
                                      cursor);
    case ZfRecordKeyPrivateIv:
        return zf_record_decode_bytes(record->private_iv, sizeof(record->private_iv), cursor);
    case ZfRecordKeySignCount:
        return zf_record_decode_counter(cursor, &record->sign_count);
    case ZfRecordKeyCreatedAt:
        return zf_record_decode_counter(cursor, &record->created_at);
    case ZfRecordKeyResidentKey:
        return zf_cbor_read_bool(cursor, &record->resident_key);
    case ZfRecordKeyCredProtect: {
        uint64_t raw = 0;
        if (!zf_cbor_read_uint(cursor, &raw) || !zf_ctap_cred_protect_value_is_valid(raw)) {
            return false;
        }
        record->cred_protect = (uint8_t)raw;
        return true;
    }
    default:
        return zf_cbor_skip(cursor);
    }
}

static bool zf_record_decode(const uint8_t *data, size_t data_size, const char *file_name,
                             ZfCredentialRecord *out_record,
                             ZfCounterFloorFile *embedded_counter_floor,
                             bool *has_embedded_counter_floor) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint32_t version = 0;
    bool saw_version = false;
    bool saw_credential_id = false;
    bool saw_rp_id = false;
    bool saw_user_id = false;
    bool saw_public_x = false;
    bool saw_public_y = false;
    bool saw_private_wrapped = false;
    bool saw_private_iv = false;
    bool saw_sign_count = false;
    bool saw_created_at = false;
    bool saw_resident_key = false;
    bool saw_cred_protect = false;
    bool saw_hmac_secret_wrapped = false;
    bool saw_hmac_secret_iv = false;
    bool saw_counter_floor = false;
    uint8_t hmac_secret_wrapped[ZF_RECORD_HMAC_SECRET_STORAGE_LEN] = {0};
    uint8_t hmac_secret_iv[ZF_WRAP_IV_LEN] = {0};
    bool ok = false;

    if (embedded_counter_floor) {
        memset(embedded_counter_floor, 0, sizeof(*embedded_counter_floor));
    }
    if (has_embedded_counter_floor) {
        *has_embedded_counter_floor = false;
    }
    if (!data || !file_name || !out_record) {
        return false;
    }
    memset(out_record, 0, sizeof(*out_record));
    strncpy(out_record->file_name, file_name, sizeof(out_record->file_name) - 1);
    out_record->credential_id_len = ZF_CREDENTIAL_ID_LEN;

    zf_cbor_cursor_init(&cursor, data, data_size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        goto cleanup;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            goto cleanup;
        }
        if (key == ZfRecordKeyHmacSecretWrapped) {
            if (saw_hmac_secret_wrapped) {
                goto cleanup;
            }
            if (!zf_record_decode_bytes(hmac_secret_wrapped, sizeof(hmac_secret_wrapped),
                                        &cursor)) {
                goto cleanup;
            }
            saw_hmac_secret_wrapped = true;
        } else if (key == ZfRecordKeyHmacSecretIv) {
            if (saw_hmac_secret_iv) {
                goto cleanup;
            }
            if (!zf_record_decode_bytes(hmac_secret_iv, sizeof(hmac_secret_iv), &cursor)) {
                goto cleanup;
            }
            saw_hmac_secret_iv = true;
        } else if (key == ZfRecordKeyCounterFloor) {
            const uint8_t *ptr = NULL;
            size_t size = 0;

            if (saw_counter_floor || !zf_cbor_read_bytes_ptr(&cursor, &ptr, &size) ||
                size != sizeof(ZfCounterFloorFile) || !embedded_counter_floor) {
                goto cleanup;
            }
            memcpy(embedded_counter_floor, ptr, sizeof(*embedded_counter_floor));
            saw_counter_floor = true;
        } else if (!zf_record_decode_field(&cursor, key, out_record, &version)) {
            goto cleanup;
        }
        switch (key) {
        case ZfRecordKeyVersion:
            saw_version = true;
            break;
        case ZfRecordKeyCredentialId:
            saw_credential_id = true;
            break;
        case ZfRecordKeyRpId:
            saw_rp_id = true;
            break;
        case ZfRecordKeyUserId:
            saw_user_id = true;
            break;
        case ZfRecordKeyPublicX:
            saw_public_x = true;
            break;
        case ZfRecordKeyPublicY:
            saw_public_y = true;
            break;
        case ZfRecordKeyPrivateWrapped:
            saw_private_wrapped = true;
            break;
        case ZfRecordKeyPrivateIv:
            saw_private_iv = true;
            break;
        case ZfRecordKeySignCount:
            saw_sign_count = true;
            break;
        case ZfRecordKeyCreatedAt:
            saw_created_at = true;
            break;
        case ZfRecordKeyResidentKey:
            saw_resident_key = true;
            break;
        case ZfRecordKeyCredProtect:
            saw_cred_protect = true;
            break;
        default:
            break;
        }
    }

    if (cursor.ptr != cursor.end) {
        goto cleanup;
    }
    if (!saw_version || !saw_credential_id || !saw_rp_id || !saw_user_id || !saw_public_x ||
        !saw_public_y || !saw_private_wrapped || !saw_private_iv || !saw_sign_count ||
        !saw_created_at || !saw_resident_key || !saw_cred_protect) {
        goto cleanup;
    }
    if (saw_hmac_secret_wrapped != saw_hmac_secret_iv) {
        goto cleanup;
    }
    if (version != ZF_STORE_VERSION || out_record->rp_id[0] == '\0' ||
        !zf_record_id_matches_file_name(out_record->credential_id, out_record->credential_id_len,
                                        file_name)) {
        goto cleanup;
    }
    if (saw_hmac_secret_wrapped &&
        !zf_record_unwrap_hmac_secret(out_record, hmac_secret_wrapped, hmac_secret_iv)) {
        goto cleanup;
    }
    out_record->hmac_secret = saw_hmac_secret_wrapped;

    out_record->storage_version = version;
    out_record->in_use = true;
    if (has_embedded_counter_floor) {
        *has_embedded_counter_floor = saw_counter_floor;
    }
    ok = true;

cleanup:
    zf_crypto_secure_zero(hmac_secret_wrapped, sizeof(hmac_secret_wrapped));
    zf_crypto_secure_zero(hmac_secret_iv, sizeof(hmac_secret_iv));
    return ok;
}

bool zf_store_record_format_is_record_name(const char *name) {
    size_t expected_len = ZF_CREDENTIAL_ID_LEN * 2;
    if (strlen(name) != expected_len) {
        return false;
    }

    for (size_t i = 0; i < expected_len; ++i) {
        char ch = name[i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool zf_store_record_format_load_record_internal_with_buffer(
    Storage *storage, const char *file_name, ZfCredentialRecord *record, uint8_t *buffer,
    size_t buffer_size, bool validate_counter_floor) {
    char path[128];
    size_t size = 0;
    ZfCounterFloorFile embedded_counter_floor;
    bool has_embedded_counter_floor = false;
    bool decoded = false;

    if (!file_name || !record || !buffer || buffer_size == 0) {
        if (buffer && buffer_size > 0) {
            zf_crypto_secure_zero(buffer, buffer_size);
        }
        return false;
    }
    zf_store_build_record_path(file_name, path, sizeof(path));
    if (!zf_storage_read_file(storage, path, buffer, buffer_size, &size)) {
        zf_crypto_secure_zero(buffer, buffer_size);
        return false;
    }

    decoded = zf_record_decode(buffer, size, file_name, record, &embedded_counter_floor,
                               &has_embedded_counter_floor);
    zf_crypto_secure_zero(buffer, buffer_size);
    if (!decoded) {
        zf_crypto_secure_zero(&embedded_counter_floor, sizeof(embedded_counter_floor));
        return false;
    }
    if (!validate_counter_floor) {
        zf_crypto_secure_zero(&embedded_counter_floor, sizeof(embedded_counter_floor));
        return true;
    }
    bool ok = zf_store_counter_floor_validate(storage, record, &embedded_counter_floor,
                                              has_embedded_counter_floor, NULL);
    zf_crypto_secure_zero(&embedded_counter_floor, sizeof(embedded_counter_floor));
    return ok;
}

bool zf_store_record_format_load_record_with_buffer(Storage *storage, const char *file_name,
                                                    ZfCredentialRecord *record, uint8_t *buffer,
                                                    size_t buffer_size) {
    return zf_store_record_format_load_record_internal_with_buffer(storage, file_name, record,
                                                                   buffer, buffer_size, true);
}

bool zf_store_record_format_load_record_for_display_with_buffer(Storage *storage,
                                                                const char *file_name,
                                                                ZfCredentialRecord *record,
                                                                uint8_t *buffer,
                                                                size_t buffer_size) {
    return zf_store_record_format_load_record_internal_with_buffer(storage, file_name, record,
                                                                   buffer, buffer_size, false);
}

bool zf_store_record_format_load_index_with_buffer(Storage *storage, const char *file_name,
                                                   ZfCredentialIndexEntry *entry, uint8_t *buffer,
                                                   size_t buffer_size) {
    char path[128];
    size_t size = 0;
    ZfCounterFloorFile embedded_counter_floor;
    bool has_embedded_counter_floor = false;
    ZfCredentialRecord record = {0};
    uint32_t counter_high_water = 0;

    if (!file_name || !entry || !buffer || buffer_size == 0) {
        return false;
    }
    zf_store_build_record_path(file_name, path, sizeof(path));
    if (!zf_storage_read_file(storage, path, buffer, buffer_size, &size)) {
        return false;
    }

    if (!zf_record_decode(buffer, size, file_name, &record, &embedded_counter_floor,
                          &has_embedded_counter_floor)) {
        zf_crypto_secure_zero(buffer, size);
        return false;
    }
    zf_crypto_secure_zero(buffer, size);
    zf_store_index_entry_from_record(&record, entry);
    zf_crypto_secure_zero(&record, sizeof(record));

    if (!zf_store_counter_floor_validate_index(storage, file_name, entry, &embedded_counter_floor,
                                               has_embedded_counter_floor, &counter_high_water)) {
        zf_crypto_secure_zero(&embedded_counter_floor, sizeof(embedded_counter_floor));
        return false;
    }
    zf_crypto_secure_zero(&embedded_counter_floor, sizeof(embedded_counter_floor));
    entry->counter_high_water = counter_high_water;
    return true;
}

/*
 * New credential records carry an embedded encrypted counter floor. Later
 * assertion counter reservations still use the companion .counter high-water
 * file so a credential-record rollback cannot lower an already-used counter.
 */
bool zf_store_record_format_write_record_with_buffer(Storage *storage,
                                                     const ZfCredentialRecord *record,
                                                     uint8_t *buffer, size_t buffer_size) {
    size_t encoded_size = 0;
    char path[128];
    char temp_path[128];
    bool ok = false;

    if (!record || !buffer || buffer_size == 0) {
        if (buffer && buffer_size > 0) {
            zf_crypto_secure_zero(buffer, buffer_size);
        }
        return false;
    }
    if (buffer_size < ZF_STORE_RECORD_MAX_SIZE) {
        goto cleanup;
    }
    if (!zf_store_record_format_encode(record, buffer, &encoded_size)) {
        goto cleanup;
    }
    zf_store_build_record_path(record->file_name, path, sizeof(path));
    zf_store_build_temp_path(record->file_name, temp_path, sizeof(temp_path));
    ok = zf_storage_write_file_atomic(storage, path, temp_path, buffer, encoded_size);

cleanup:
    zf_crypto_secure_zero(buffer, buffer_size);
    return ok;
}
