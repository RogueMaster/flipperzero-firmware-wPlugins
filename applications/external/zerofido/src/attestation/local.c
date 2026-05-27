/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "local.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <flipper_format/flipper_format.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#include "../zerofido_crypto.h"
#include "../zerofido_storage.h"
#include "../zerofido_telemetry.h"

#define ZF_LOCAL_ATTESTATION_KEY_FILE_VERSION 1U
#define ZF_LOCAL_ATTESTATION_KEY_TYPE_USER 1U
#define ZF_LOCAL_ATTESTATION_TBS_MAX_SIZE 512U
#define ZF_LOCAL_ATTESTATION_SIGNATURE_MAX_SIZE 80U
#define ZF_LOCAL_ATTESTATION_BIT_STRING_MAX_SIZE (ZF_LOCAL_ATTESTATION_SIGNATURE_MAX_SIZE + 1U)

typedef struct {
    uint8_t *data;
    size_t len;
    size_t capacity;
} ZfDerBuffer;

typedef struct {
    uint8_t tbs[ZF_LOCAL_ATTESTATION_TBS_MAX_SIZE];
    uint8_t cert[ZF_ATTESTATION_CERT_MAX_SIZE];
    uint8_t spki[128];
    uint8_t bit_string[ZF_LOCAL_ATTESTATION_BIT_STRING_MAX_SIZE];
} ZfLocalAttestationScratch;

_Static_assert(sizeof(ZfLocalAttestationScratch) <= 1536U,
               "local attestation generation scratch must stay NFC-safe");

/*
 * This module writes a constrained DER/X.509 profile for local attestation.
 * It is intentionally profile-driven rather than a general-purpose certificate
 * library, which keeps heap use and encoded size bounded on-device.
 */
static bool zf_der_append(ZfDerBuffer *buffer, const uint8_t *data, size_t data_len) {
    if (!buffer || buffer->len > buffer->capacity || (!data && data_len > 0U) ||
        data_len > buffer->capacity - buffer->len) {
        return false;
    }
    if (data_len > 0U) {
        memcpy(&buffer->data[buffer->len], data, data_len);
        buffer->len += data_len;
    }
    return true;
}

static size_t zf_der_length_size(size_t value_len) {
    if (value_len < 0x80U) {
        return 1U;
    }
    if (value_len <= 0xFFU) {
        return 2U;
    }
    return 3U;
}

static bool zf_der_append_length(ZfDerBuffer *buffer, size_t value_len) {
    uint8_t len_bytes[3];
    size_t len_size = zf_der_length_size(value_len);

    if (!buffer || len_size > sizeof(len_bytes)) {
        return false;
    }
    if (len_size == 1U) {
        len_bytes[0] = (uint8_t)value_len;
    } else if (len_size == 2U) {
        len_bytes[0] = 0x81U;
        len_bytes[1] = (uint8_t)value_len;
    } else {
        len_bytes[0] = 0x82U;
        len_bytes[1] = (uint8_t)(value_len >> 8);
        len_bytes[2] = (uint8_t)value_len;
    }
    return zf_der_append(buffer, len_bytes, len_size);
}

static bool zf_der_write_length(uint8_t *out, size_t out_capacity, size_t value_len,
                                size_t *written) {
    size_t len_size = zf_der_length_size(value_len);

    if (!out || !written || len_size > out_capacity || len_size > 3U) {
        return false;
    }
    if (len_size == 1U) {
        out[0] = (uint8_t)value_len;
    } else if (len_size == 2U) {
        out[0] = 0x81U;
        out[1] = (uint8_t)value_len;
    } else {
        out[0] = 0x82U;
        out[1] = (uint8_t)(value_len >> 8);
        out[2] = (uint8_t)value_len;
    }
    *written = len_size;
    return true;
}

static bool zf_der_wrap_tlv_in_place(ZfDerBuffer *buffer, uint8_t tag) {
    size_t length_len = 0U;
    size_t header_len = 0U;

    if (!buffer || !buffer->data || buffer->len > buffer->capacity) {
        return false;
    }

    length_len = zf_der_length_size(buffer->len);
    header_len = 1U + length_len;
    if (header_len > buffer->capacity || buffer->len > buffer->capacity - header_len) {
        return false;
    }
    memmove(buffer->data + header_len, buffer->data, buffer->len);
    buffer->data[0] = tag;
    if (!zf_der_write_length(buffer->data + 1U, length_len, buffer->len, &length_len)) {
        return false;
    }
    buffer->len += header_len;
    return true;
}

static bool zf_der_append_tlv(ZfDerBuffer *buffer, uint8_t tag, const uint8_t *value,
                              size_t value_len) {
    return zf_der_append(buffer, &tag, 1U) && zf_der_append_length(buffer, value_len) &&
           zf_der_append(buffer, value, value_len);
}

static bool zf_der_append_integer(ZfDerBuffer *buffer, const uint8_t *value, size_t value_len) {
    uint8_t encoded[ZF_PRIVATE_KEY_LEN + 1U];
    size_t offset = 0U;
    size_t encoded_len = 0U;

    if (!buffer || !value || value_len == 0U || value_len > ZF_PRIVATE_KEY_LEN) {
        return false;
    }
    while (offset + 1U < value_len && value[offset] == 0U) {
        offset++;
    }
    if ((value[offset] & 0x80U) != 0U) {
        encoded[encoded_len++] = 0U;
    }
    memcpy(&encoded[encoded_len], &value[offset], value_len - offset);
    encoded_len += value_len - offset;
    return zf_der_append_tlv(buffer, 0x02U, encoded, encoded_len);
}

static bool zf_der_append_bit_string(ZfDerBuffer *buffer, const uint8_t *value, size_t value_len,
                                     uint8_t *scratch, size_t scratch_capacity) {
    if (!buffer || !scratch || scratch_capacity == 0U || (!value && value_len > 0U) ||
        value_len > scratch_capacity - 1U) {
        return false;
    }
    scratch[0] = 0U;
    if (value_len > 0U) {
        memcpy(&scratch[1], value, value_len);
    }
    return zf_der_append_tlv(buffer, 0x03U, scratch, value_len + 1U);
}

static bool zf_der_parse_length(const uint8_t *input, size_t input_len, size_t *header_len,
                                size_t *value_len) {
    if (!input || input_len < 2 || !header_len || !value_len) {
        return false;
    }
    if ((input[1] & 0x80U) == 0) {
        *header_len = 2;
        *value_len = input[1];
        return *value_len <= input_len - *header_len;
    }

    size_t length_octets = input[1] & 0x7FU;
    if (length_octets == 0 || length_octets > sizeof(size_t) || length_octets > input_len - 2U) {
        return false;
    }

    size_t length = 0;
    for (size_t i = 0; i < length_octets; ++i) {
        length = (length << 8) | input[2 + i];
    }
    *header_len = 2 + length_octets;
    *value_len = length;
    return *value_len <= input_len - *header_len;
}

static bool zf_der_read_element(const uint8_t *input, size_t input_len, uint8_t expected_tag,
                                const uint8_t **value, size_t *value_len, size_t *element_len) {
    size_t header_len = 0;

    if (!input || input_len < 2 || !value || !value_len || !element_len ||
        input[0] != expected_tag ||
        !zf_der_parse_length(input, input_len, &header_len, value_len)) {
        return false;
    }

    *value = input + header_len;
    if (*value_len > SIZE_MAX - header_len) {
        return false;
    }
    *element_len = header_len + *value_len;
    return true;
}

static bool zf_contains(const uint8_t *haystack, size_t haystack_len, const uint8_t *needle,
                        size_t needle_len) {
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len) {
        return false;
    }
    for (size_t i = 0; i <= haystack_len - needle_len; ++i) {
        if (memcmp(&haystack[i], needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool zf_profile_is_valid(const ZfLocalAttestationProfile *profile) {
    return profile && profile->data_dir && profile->assets_dir && profile->cert_file &&
           profile->cert_temp_file && profile->key_file && profile->key_temp_file &&
           profile->key_file_type && profile->subject_der && profile->subject_der_len > 0U &&
           profile->extensions_der && profile->extensions_der_len > 0U;
}

static bool zf_local_attestation_ensure_directories(const ZfLocalAttestationProfile *profile,
                                                    Storage *storage) {
    if (!zf_profile_is_valid(profile) || !storage) {
        return false;
    }
    if (!zf_storage_ensure_app_data_dir(storage)) {
        return false;
    }
    if (!zf_storage_ensure_dir(storage, profile->data_dir)) {
        return false;
    }
    if (!zf_storage_ensure_dir(storage, profile->assets_dir)) {
        return false;
    }
    return true;
}

static bool zf_local_attestation_remove_paths(const ZfLocalAttestationProfile *profile,
                                              Storage *storage, bool remove_committed) {
    const char *paths[2];
    size_t count = 0U;

    if (!zf_profile_is_valid(profile)) {
        return false;
    }
    if (remove_committed) {
        return zf_storage_remove_atomic_file(storage, profile->cert_file,
                                             profile->cert_temp_file) &&
               zf_storage_remove_atomic_file(storage, profile->key_file, profile->key_temp_file);
    }
    paths[count++] = profile->cert_temp_file;
    paths[count++] = profile->key_temp_file;
    return zf_storage_remove_optional_paths(storage, paths, count);
}

static void zf_local_attestation_cleanup_temps(const ZfLocalAttestationProfile *profile,
                                               Storage *storage) {
    zf_storage_recover_atomic_file(storage, profile->cert_file, profile->cert_temp_file);
    zf_storage_recover_atomic_file(storage, profile->key_file, profile->key_temp_file);
    if (!zf_local_attestation_remove_paths(profile, storage, false)) {
        zf_telemetry_log("attestation temp cleanup failed");
    }
}

static void zf_local_attestation_cleanup_pair(const ZfLocalAttestationProfile *profile,
                                              Storage *storage) {
    if (!zf_local_attestation_remove_paths(profile, storage, true)) {
        zf_telemetry_log("attestation pair cleanup failed");
    }
}

bool zf_local_attestation_get_cert_size(const ZfLocalAttestationProfile *profile, size_t *out_len) {
    bool ok = false;
    Storage *storage = NULL;
    File *file = NULL;

    if (!zf_profile_is_valid(profile) || !out_len) {
        return false;
    }
    *out_len = 0;

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return false;
    }
    file = storage_file_alloc(storage);
    if (!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    if (storage_file_open(file, profile->cert_file, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t file_size = storage_file_size(file);
        if (file_size > 0U && file_size <= ZF_ATTESTATION_CERT_MAX_SIZE) {
            *out_len = file_size;
            ok = true;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* Loads the public attestation certificate DER without exposing private material. */
bool zf_local_attestation_load_cert(const ZfLocalAttestationProfile *profile, uint8_t *out,
                                    size_t out_capacity, size_t *out_len) {
    bool ok = false;
    Storage *storage = NULL;
    size_t file_size = 0U;

    if (!zf_profile_is_valid(profile) || !out || !out_len) {
        return false;
    }
    *out_len = 0;

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return false;
    }
    if (zf_storage_read_file(storage, profile->cert_file, out, out_capacity, &file_size)) {
        *out_len = file_size;
        ok = true;
    }

    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void zf_local_attestation_cleanup_storage(const ZfLocalAttestationProfile *profile,
                                                 bool remove_committed) {
    Storage *storage = NULL;

    if (!zf_profile_is_valid(profile)) {
        return;
    }
    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return;
    }
    if (remove_committed) {
        zf_local_attestation_cleanup_pair(profile, storage);
    } else {
        zf_local_attestation_cleanup_temps(profile, storage);
    }
    furi_record_close(RECORD_STORAGE);
}

/* Encrypts and persists the generated attestation private key. */
static bool zf_encrypt_private_key(const ZfLocalAttestationProfile *profile, Storage *storage,
                                   uint8_t *private_key) {
    bool ok = false;

    if (!zf_profile_is_valid(profile) || !storage || !private_key) {
        return false;
    }

    const ZfStorageEncryptedBlobWriteSpec spec = {
        .file_type = profile->key_file_type,
        .version = ZF_LOCAL_ATTESTATION_KEY_FILE_VERSION,
        .has_type = true,
        .type = ZF_LOCAL_ATTESTATION_KEY_TYPE_USER,
        .key_slot = FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT,
        .plaintext = private_key,
        .plaintext_len = ZF_PRIVATE_KEY_LEN,
        .encrypted_len = 48U,
    };
    ok = zf_storage_write_encrypted_blob_atomic(storage, profile->key_file, profile->key_temp_file,
                                                &spec);
    if (!ok) {
        zf_telemetry_log("attestation key write failed");
    }
    return ok;
}

/* Loads and decrypts the local attestation private key into caller-owned memory. */
bool zf_local_attestation_load_private_key(const ZfLocalAttestationProfile *profile,
                                           uint8_t private_key[ZF_PRIVATE_KEY_LEN]) {
    bool ok = false;
    Storage *storage = NULL;
    const uint32_t accepted_versions[] = {ZF_LOCAL_ATTESTATION_KEY_FILE_VERSION};

    if (!zf_profile_is_valid(profile) || !private_key) {
        return false;
    }
    zf_crypto_secure_zero(private_key, ZF_PRIVATE_KEY_LEN);

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return false;
    }

    const ZfStorageEncryptedBlobReadSpec spec = {
        .file_type = profile->key_file_type,
        .accepted_versions = accepted_versions,
        .accepted_version_count = 1U,
        .has_type = true,
        .expected_type = ZF_LOCAL_ATTESTATION_KEY_TYPE_USER,
        .key_slot = FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT,
        .plaintext = private_key,
        .plaintext_len = ZF_PRIVATE_KEY_LEN,
        .encrypted_len = 48U,
    };
    ok = zf_storage_read_encrypted_blob(storage, profile->key_file, &spec);
    furi_record_close(RECORD_STORAGE);
    if (!ok) {
        zf_crypto_secure_zero(private_key, ZF_PRIVATE_KEY_LEN);
    }
    return ok;
}

/* Extracts the subjectPublicKeyInfo EC point from the local attestation cert shape. */
bool zf_local_attestation_extract_cert_public_key(
    const uint8_t *cert, size_t cert_len, uint8_t public_key[ZF_LOCAL_ATTESTATION_EC_POINT_SIZE]) {
    const uint8_t *certificate_value = NULL;
    const uint8_t *tbs_value = NULL;
    const uint8_t *spki_value = NULL;
    const uint8_t *element_value = NULL;
    const uint8_t *bit_string_value = NULL;
    size_t certificate_value_len = 0;
    size_t tbs_value_len = 0;
    size_t spki_value_len = 0;
    size_t element_value_len = 0;
    size_t bit_string_value_len = 0;
    size_t element_len = 0;
    size_t offset = 0;
    size_t tbs_offset = 0;

    if (!cert || !public_key ||
        !zf_der_read_element(cert, cert_len, 0x30, &certificate_value, &certificate_value_len,
                             &element_len) ||
        element_len != cert_len) {
        return false;
    }
    if (!zf_der_read_element(certificate_value, certificate_value_len, 0x30, &tbs_value,
                             &tbs_value_len, &element_len)) {
        return false;
    }
    offset += element_len;
    if (!zf_der_read_element(certificate_value + offset, certificate_value_len - offset, 0x30,
                             &element_value, &element_value_len, &element_len)) {
        return false;
    }
    offset += element_len;
    if (!zf_der_read_element(certificate_value + offset, certificate_value_len - offset, 0x03,
                             &bit_string_value, &bit_string_value_len, &element_len)) {
        return false;
    }
    offset += element_len;
    if (offset != certificate_value_len) {
        return false;
    }

    if (tbs_offset < tbs_value_len && tbs_value[tbs_offset] == 0xA0) {
        if (!zf_der_read_element(tbs_value + tbs_offset, tbs_value_len - tbs_offset, 0xA0,
                                 &element_value, &element_value_len, &element_len)) {
            return false;
        }
        tbs_offset += element_len;
    }
    for (size_t i = 0; i < 5; ++i) {
        uint8_t tag = i == 0 ? 0x02U : 0x30U;
        if (!zf_der_read_element(tbs_value + tbs_offset, tbs_value_len - tbs_offset, tag,
                                 &element_value, &element_value_len, &element_len)) {
            return false;
        }
        tbs_offset += element_len;
    }
    if (!zf_der_read_element(tbs_value + tbs_offset, tbs_value_len - tbs_offset, 0x30, &spki_value,
                             &spki_value_len, &element_len)) {
        return false;
    }
    tbs_offset += element_len;
    if (tbs_offset < tbs_value_len) {
        if (!zf_der_read_element(tbs_value + tbs_offset, tbs_value_len - tbs_offset, 0xA3,
                                 &element_value, &element_value_len, &element_len)) {
            return false;
        }
        tbs_offset += element_len;
    }
    if (tbs_offset != tbs_value_len) {
        return false;
    }

    offset = 0;
    if (!zf_der_read_element(spki_value, spki_value_len, 0x30, &element_value, &element_value_len,
                             &element_len)) {
        return false;
    }
    offset += element_len;
    if (!zf_der_read_element(spki_value + offset, spki_value_len - offset, 0x03, &bit_string_value,
                             &bit_string_value_len, &element_len)) {
        return false;
    }
    offset += element_len;
    if (offset != spki_value_len ||
        bit_string_value_len != ZF_LOCAL_ATTESTATION_EC_POINT_SIZE + 1U ||
        bit_string_value[0] != 0x00 || bit_string_value[1] != 0x04) {
        return false;
    }

    memcpy(public_key, bit_string_value + 1, ZF_LOCAL_ATTESTATION_EC_POINT_SIZE);
    return true;
}

bool zf_local_attestation_private_key_matches_cert(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                                   const uint8_t *cert, size_t cert_len,
                                                   const uint8_t *identity, size_t identity_len) {
    uint8_t cert_public_key[ZF_LOCAL_ATTESTATION_EC_POINT_SIZE];
    uint8_t public_x[ZF_PUBLIC_KEY_LEN];
    uint8_t public_y[ZF_PUBLIC_KEY_LEN];
    bool ok = false;

    if (!private_key || !cert || cert_len == 0U || (identity_len > 0U && !identity) ||
        !zf_local_attestation_extract_cert_public_key(cert, cert_len, cert_public_key) ||
        !zf_crypto_compute_public_key_from_private(private_key, public_x, public_y)) {
        return false;
    }

    ok = cert_public_key[0] == 0x04 &&
         memcmp(&cert_public_key[1], public_x, sizeof(public_x)) == 0 &&
         memcmp(&cert_public_key[1 + ZF_PUBLIC_KEY_LEN], public_y, sizeof(public_y)) == 0 &&
         (identity_len == 0U || zf_contains(cert, cert_len, identity, identity_len));
    zf_crypto_secure_zero(cert_public_key, sizeof(cert_public_key));
    zf_crypto_secure_zero(public_x, sizeof(public_x));
    zf_crypto_secure_zero(public_y, sizeof(public_y));
    return ok;
}

/* Confirms persisted cert/key consistency and optional profile identity bytes. */
static bool zf_private_key_matches_cert(const ZfLocalAttestationProfile *profile,
                                        const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                        uint8_t *cert, size_t cert_capacity) {
    size_t cert_len = 0;

    if (!profile || !private_key || !cert || cert_capacity == 0U ||
        cert_capacity > ZF_ATTESTATION_CERT_MAX_SIZE) {
        return false;
    }

    if (!zf_local_attestation_load_cert(profile, cert, cert_capacity, &cert_len)) {
        zf_crypto_secure_zero(cert, cert_capacity);
        return false;
    }

    bool ok = zf_local_attestation_private_key_matches_cert(
        private_key, cert, cert_len, profile->identity, profile->identity_len);
    zf_crypto_secure_zero(cert, cert_capacity);
    return ok;
}

/* Reuses the project P-256 generator and converts the public key to X9.62 form. */
static bool zf_generate_keypair(uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                uint8_t public_key[ZF_LOCAL_ATTESTATION_EC_POINT_SIZE]) {
    ZfP256KeyAgreementKey key = {0};

    if (!private_key || !public_key || !zf_crypto_generate_key_agreement_key(&key)) {
        return false;
    }

    memcpy(private_key, key.private_key, ZF_PRIVATE_KEY_LEN);
    public_key[0] = 0x04U;
    memcpy(&public_key[1], key.public_x, ZF_PUBLIC_KEY_LEN);
    memcpy(&public_key[1 + ZF_PUBLIC_KEY_LEN], key.public_y, ZF_PUBLIC_KEY_LEN);
    zf_crypto_secure_zero(&key, sizeof(key));
    return true;
}

/*
 * Builds the DER TBSCertificate for local self-signed attestation. This is a
 * constrained DER writer for this fixed profile, not a general X.509 encoder.
 */
static bool zf_build_tbs(const ZfLocalAttestationProfile *profile,
                         const uint8_t public_key[ZF_LOCAL_ATTESTATION_EC_POINT_SIZE], uint8_t *tbs,
                         size_t tbs_capacity, size_t *tbs_len, ZfLocalAttestationScratch *scratch) {
    static const uint8_t version[] = {0x02, 0x01, 0x02};
    static const uint8_t ecdsa_sha256_alg[] = {
        0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02,
    };
    static const uint8_t validity[] = {
        0x30, 0x1E, 0x17, 0x0D, '2', '6', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0',
        'Z',  0x17, 0x0D, '3',  '6', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z',
    };
    static const uint8_t public_key_alg[] = {
        0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,
    };
    uint8_t serial[16];
    ZfDerBuffer body = {.data = tbs, .capacity = tbs_capacity};
    ZfDerBuffer spki = {.data = scratch ? scratch->spki : NULL,
                        .capacity = scratch ? sizeof(scratch->spki) : 0U};

    if (!zf_profile_is_valid(profile) || !public_key || !tbs || !tbs_len || !scratch) {
        return false;
    }

    furi_hal_random_fill_buf(serial, sizeof(serial));
    serial[0] &= 0x7FU;
    if (serial[0] == 0U) {
        serial[0] = 1U;
    }

    if (!zf_der_append_tlv(&body, 0xA0U, version, sizeof(version)) ||
        !zf_der_append_integer(&body, serial, sizeof(serial)) ||
        !zf_der_append(&body, ecdsa_sha256_alg, sizeof(ecdsa_sha256_alg)) ||
        !zf_der_append(&body, profile->subject_der, profile->subject_der_len) ||
        !zf_der_append(&body, validity, sizeof(validity)) ||
        !zf_der_append(&body, profile->subject_der, profile->subject_der_len) ||
        !zf_der_append(&spki, public_key_alg, sizeof(public_key_alg)) ||
        !zf_der_append_bit_string(&spki, public_key, ZF_LOCAL_ATTESTATION_EC_POINT_SIZE,
                                  scratch->bit_string, sizeof(scratch->bit_string)) ||
        !zf_der_append_tlv(&body, 0x30U, spki.data, spki.len) ||
        !zf_der_append_tlv(&body, 0xA3U, profile->extensions_der, profile->extensions_der_len)) {
        return false;
    }

    if (!zf_der_wrap_tlv_in_place(&body, 0x30U)) {
        return false;
    }
    *tbs_len = body.len;
    return true;
}

static bool zf_build_cert(const ZfLocalAttestationProfile *profile,
                          const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                          const uint8_t public_key[ZF_LOCAL_ATTESTATION_EC_POINT_SIZE],
                          uint8_t *cert, size_t cert_capacity, size_t *cert_len,
                          ZfLocalAttestationScratch *scratch) {
    static const uint8_t ecdsa_sha256_alg[] = {
        0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02,
    };
    uint8_t hash[32];
    uint8_t signature[ZF_LOCAL_ATTESTATION_SIGNATURE_MAX_SIZE];
    size_t tbs_len = 0U;
    size_t signature_len = 0U;
    ZfDerBuffer body = {.data = cert, .capacity = cert_capacity};

    if (!private_key || !public_key || !cert || !cert_len || !scratch ||
        !zf_build_tbs(profile, public_key, scratch->tbs, sizeof(scratch->tbs), &tbs_len, scratch)) {
        return false;
    }

    zf_crypto_sha256(scratch->tbs, tbs_len, hash);
    if (!zf_crypto_sign_hash_with_private_key(private_key, hash, signature, sizeof(signature),
                                              &signature_len)) {
        zf_crypto_secure_zero(hash, sizeof(hash));
        zf_crypto_secure_zero(signature, sizeof(signature));
        return false;
    }

    bool ok = zf_der_append(&body, scratch->tbs, tbs_len) &&
              zf_der_append(&body, ecdsa_sha256_alg, sizeof(ecdsa_sha256_alg)) &&
              zf_der_append_bit_string(&body, signature, signature_len, scratch->bit_string,
                                       sizeof(scratch->bit_string)) &&
              zf_der_wrap_tlv_in_place(&body, 0x30U);
    if (ok) {
        *cert_len = body.len;
    }

    zf_crypto_secure_zero(hash, sizeof(hash));
    zf_crypto_secure_zero(signature, sizeof(signature));
    return ok;
}

/* Writes cert first, then encrypted key; a key-write failure removes the cert. */
static bool zf_bootstrap_assets(const ZfLocalAttestationProfile *profile, const uint8_t *cert,
                                size_t cert_len, const uint8_t private_key[ZF_PRIVATE_KEY_LEN]) {
    bool ok = false;
    uint8_t key_copy[ZF_PRIVATE_KEY_LEN];
    Storage *storage = NULL;

    if (!zf_profile_is_valid(profile) || !cert || cert_len == 0U ||
        cert_len > ZF_ATTESTATION_CERT_MAX_SIZE || !private_key) {
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        return false;
    }

    do {
        if (!zf_local_attestation_ensure_directories(profile, storage)) {
            zf_telemetry_log("attestation dirs failed");
            zf_local_attestation_cleanup_pair(profile, storage);
            break;
        }
        zf_local_attestation_cleanup_temps(profile, storage);
        if (!zf_storage_write_file_atomic(storage, profile->cert_file, profile->cert_temp_file,
                                          cert, cert_len)) {
            zf_telemetry_log("attestation cert write failed");
            zf_local_attestation_cleanup_pair(profile, storage);
            break;
        }
        memcpy(key_copy, private_key, sizeof(key_copy));
        if (!zf_encrypt_private_key(profile, storage, key_copy)) {
            zf_crypto_secure_zero(key_copy, sizeof(key_copy));
            zf_local_attestation_cleanup_pair(profile, storage);
            break;
        }
        zf_crypto_secure_zero(key_copy, sizeof(key_copy));
        ok = true;
    } while (false);

    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* Generates the full local attestation asset pair for a profile. */
static bool zf_generate_assets(const ZfLocalAttestationProfile *profile) {
    uint8_t private_key[ZF_PRIVATE_KEY_LEN];
    uint8_t public_key[ZF_LOCAL_ATTESTATION_EC_POINT_SIZE];
    ZfLocalAttestationScratch *scratch = malloc(sizeof(*scratch));
    size_t cert_len = 0U;
    bool ok = false;

    if (!scratch) {
        zf_telemetry_log_oom("attestation generate assets", sizeof(*scratch));
        return false;
    }
    memset(scratch, 0, sizeof(*scratch));

    do {
        if (!zf_generate_keypair(private_key, public_key)) {
            zf_telemetry_log("attestation keygen failed");
            break;
        }
        if (!zf_build_cert(profile, private_key, public_key, scratch->cert, sizeof(scratch->cert),
                           &cert_len, scratch)) {
            zf_telemetry_log("attestation cert build failed");
            break;
        }
        ok = zf_bootstrap_assets(profile, scratch->cert, cert_len, private_key);
    } while (false);

    zf_crypto_secure_zero(private_key, sizeof(private_key));
    zf_crypto_secure_zero(public_key, sizeof(public_key));
    zf_crypto_secure_zero(scratch, sizeof(*scratch));
    free(scratch);
    return ok;
}

/* A profile is ready only if the cert and encrypted key match each other. */
static bool zf_assets_ready(const ZfLocalAttestationProfile *profile) {
    uint8_t private_key[ZF_PRIVATE_KEY_LEN];
    uint8_t *cert = malloc(ZF_ATTESTATION_CERT_MAX_SIZE);
    bool ready = false;
    bool key_loaded = false;

    if (!cert) {
        zf_telemetry_log_oom("attestation cert scratch", ZF_ATTESTATION_CERT_MAX_SIZE);
        zf_crypto_secure_zero(private_key, sizeof(private_key));
        return false;
    }
    key_loaded = zf_local_attestation_load_private_key(profile, private_key);
    if (!key_loaded) {
        zf_telemetry_log("attestation key load failed");
    } else {
        ready =
            zf_private_key_matches_cert(profile, private_key, cert, ZF_ATTESTATION_CERT_MAX_SIZE);
        if (!ready) {
            zf_telemetry_log("attestation cert key mismatch");
        }
    }
    zf_crypto_secure_zero(private_key, sizeof(private_key));
    zf_crypto_secure_zero(cert, ZF_ATTESTATION_CERT_MAX_SIZE);
    free(cert);
    return ready;
}

/* Lazy initialization entry point used by CTAP and U2F attestation paths. */
bool zf_local_attestation_ensure_assets(const ZfLocalAttestationProfile *profile) {
    zf_local_attestation_cleanup_storage(profile, false);
    if (zf_assets_ready(profile)) {
        return true;
    }

    zf_telemetry_log("attestation assets rebuilding");
    zf_local_attestation_cleanup_storage(profile, true);
    if (zf_generate_assets(profile) && zf_assets_ready(profile)) {
        zf_local_attestation_cleanup_storage(profile, false);
        return true;
    }

    zf_telemetry_log("attestation assets incomplete");
    zf_local_attestation_cleanup_storage(profile, true);
    return false;
}
