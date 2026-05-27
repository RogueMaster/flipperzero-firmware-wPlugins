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

#include "zerofido_attestation.h"

#if ZF_PACKED_ATTESTATION
#include "attestation/local.h"
#include "zerofido_crypto.h"
#include "zerofido_telemetry.h"
#endif

#if ZF_PACKED_ATTESTATION
#define ZF_ATTESTATION_DATA_DIR ZF_APP_DATA_DIR "/fido2"
#define ZF_ATTESTATION_ASSETS_DIR ZF_ATTESTATION_DATA_DIR "/attestation"
#define ZF_ATTESTATION_CERT_FILE ZF_ATTESTATION_ASSETS_DIR "/cert.der"
#define ZF_ATTESTATION_CERT_FILE_TMP ZF_ATTESTATION_ASSETS_DIR "/cert.der.tmp"
#define ZF_ATTESTATION_CERT_KEY_FILE ZF_ATTESTATION_ASSETS_DIR "/cert_key.fido2"
#define ZF_ATTESTATION_CERT_KEY_FILE_TMP ZF_ATTESTATION_ASSETS_DIR "/cert_key.fido2.tmp"

#define ZF_ATTESTATION_CERT_KEY_FILE_TYPE "ZeroFIDO FIDO2 Attestation Key File"
#endif

static const uint8_t zf_attestation_aaguid[ZF_AAGUID_LEN] = {
    0xb5, 0x1a, 0x97, 0x6a, 0x0b, 0x02, 0x40, 0xaa, 0x9d, 0x8a, 0x36, 0xc8, 0xb9, 0x1b, 0xbd, 0x1a,
};
static const char zf_attestation_aaguid_string[] = "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a";
static int8_t zf_attestation_consistency_cache = -1;
#if ZF_PACKED_ATTESTATION
static size_t zf_attestation_leaf_cert_len_cache = 0;

static const uint8_t zf_attestation_local_subject[] = {
    0x30, 0x72, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 'B',  'G',
    0x31, 0x11, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x08, 'Z',  'e',  'r',  'o',
    'F',  'I',  'D',  'O',  0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x0C, 0x19,
    'A',  'u',  't',  'h',  'e',  'n',  't',  'i',  'c',  'a',  't',  'o',  'r',  ' ',  'A',
    't',  't',  'e',  's',  't',  'a',  't',  'i',  'o',  'n',  0x31, 0x2C, 0x30, 0x2A, 0x06,
    0x03, 0x55, 0x04, 0x03, 0x0C, 0x23, 'Z',  'e',  'r',  'o',  'F',  'I',  'D',  'O',  ' ',
    'L',  'o',  'c',  'a',  'l',  ' ',  'S',  'o',  'f',  't',  'w',  'a',  'r',  'e',  ' ',
    'A',  't',  't',  'e',  's',  't',  'a',  't',  'i',  'o',  'n',
};

static const uint8_t zf_attestation_local_extensions[] = {
    0x30, 0x41, 0x30, 0x0C, 0x06, 0x03, 0x55, 0x1D, 0x13, 0x01, 0x01, 0xFF, 0x04, 0x02,
    0x30, 0x00, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04,
    0x03, 0x02, 0x07, 0x80, 0x30, 0x21, 0x06, 0x0B, 0x2B, 0x06, 0x01, 0x04, 0x01, 0x82,
    0xE5, 0x1C, 0x01, 0x01, 0x04, 0x04, 0x12, 0x04, 0x10, 0xb5, 0x1a, 0x97, 0x6a, 0x0b,
    0x02, 0x40, 0xaa, 0x9d, 0x8a, 0x36, 0xc8, 0xb9, 0x1b, 0xbd, 0x1a,
};

static const ZfLocalAttestationProfile zf_attestation_local_profile = {
    .data_dir = ZF_ATTESTATION_DATA_DIR,
    .assets_dir = ZF_ATTESTATION_ASSETS_DIR,
    .cert_file = ZF_ATTESTATION_CERT_FILE,
    .cert_temp_file = ZF_ATTESTATION_CERT_FILE_TMP,
    .key_file = ZF_ATTESTATION_CERT_KEY_FILE,
    .key_temp_file = ZF_ATTESTATION_CERT_KEY_FILE_TMP,
    .key_file_type = ZF_ATTESTATION_CERT_KEY_FILE_TYPE,
    .subject_der = zf_attestation_local_subject,
    .subject_der_len = sizeof(zf_attestation_local_subject),
    .extensions_der = zf_attestation_local_extensions,
    .extensions_der_len = sizeof(zf_attestation_local_extensions),
    .identity = zf_attestation_aaguid,
    .identity_len = sizeof(zf_attestation_aaguid),
};
#endif

const uint8_t *zf_attestation_get_aaguid(void) {
    return zf_attestation_aaguid;
}

const char *zf_attestation_get_aaguid_string(void) {
    return zf_attestation_aaguid_string;
}

#if ZF_PACKED_ATTESTATION
bool zf_attestation_get_leaf_cert_der_len(size_t *out_len) {
    size_t cert_len = 0;

    if (!out_len) {
        return false;
    }
    if (zf_attestation_leaf_cert_len_cache > 0U) {
        *out_len = zf_attestation_leaf_cert_len_cache;
        return true;
    }
    if (!zf_local_attestation_get_cert_size(&zf_attestation_local_profile, &cert_len) ||
        cert_len == 0U || cert_len > ZF_ATTESTATION_CERT_MAX_SIZE) {
        *out_len = 0;
        return false;
    }
    zf_attestation_leaf_cert_len_cache = cert_len;
    *out_len = cert_len;
    return true;
}

bool zf_attestation_ensure_ready(void) {
    bool ok = false;

    if (zf_attestation_consistency_cache > 0) {
        return zf_attestation_consistency_cache != 0;
    }

    zf_telemetry_log("attestation ensure before");
    ok = zf_local_attestation_ensure_assets(&zf_attestation_local_profile);
    if (ok) {
        size_t cert_len = 0;
        ok = zf_attestation_get_leaf_cert_der_len(&cert_len);
    }
    if (ok) {
        zf_attestation_consistency_cache = 1;
    }
    zf_telemetry_log(ok ? "attestation ensure after ok" : "attestation ensure after failed");
    return ok;
}

bool zf_attestation_load_leaf_cert_der(uint8_t *out, size_t out_capacity, size_t *out_len) {
    bool ok = false;

    if (!out || !out_len) {
        return false;
    }
    zf_telemetry_log("attestation cert before");
    ok = zf_local_attestation_load_cert(&zf_attestation_local_profile, out, out_capacity, out_len);
    zf_telemetry_log(ok ? "attestation cert after ok" : "attestation cert after failed");
    return ok;
}

bool zf_attestation_sign_input(const uint8_t *input, size_t input_len, uint8_t *out,
                               size_t out_capacity, size_t *out_len) {
    uint8_t hash[32];
    uint8_t private_key[ZF_PRIVATE_KEY_LEN];
    bool ok = false;

    if (!input || input_len == 0 || !out || !out_len) {
        return false;
    }

    zf_telemetry_log("attestation sign before");
    zf_crypto_sha256(input, input_len, hash);
    ok = zf_local_attestation_load_private_key(&zf_attestation_local_profile, private_key) &&
         zf_crypto_sign_hash_with_private_key(private_key, hash, out, out_capacity, out_len);
    zf_crypto_secure_zero(private_key, sizeof(private_key));
    zf_crypto_secure_zero(hash, sizeof(hash));
    zf_telemetry_log(ok ? "attestation sign after ok" : "attestation sign after failed");
    return ok;
}

bool zf_attestation_sign_parts(const uint8_t *first, size_t first_len, const uint8_t *second,
                               size_t second_len, uint8_t *out, size_t out_capacity,
                               size_t *out_len) {
    uint8_t hash[32];
    uint8_t private_key[ZF_PRIVATE_KEY_LEN];
    bool ok = false;

    if ((!first && first_len > 0U) || (!second && second_len > 0U) ||
        first_len + second_len == 0U || !out || !out_len) {
        return false;
    }

    zf_telemetry_log("attestation sign before");
    zf_crypto_sha256_concat(first, first_len, second, second_len, hash);
    ok = zf_local_attestation_load_private_key(&zf_attestation_local_profile, private_key) &&
         zf_crypto_sign_hash_with_private_key(private_key, hash, out, out_capacity, out_len);
    zf_crypto_secure_zero(private_key, sizeof(private_key));
    zf_crypto_secure_zero(hash, sizeof(hash));
    zf_telemetry_log(ok ? "attestation sign after ok" : "attestation sign after failed");
    return ok;
}
#else
bool zf_attestation_get_leaf_cert_der_len(size_t *out_len) {
    if (out_len) {
        *out_len = 0;
    }
    return false;
}

bool zf_attestation_ensure_ready(void) {
    return false;
}

bool zf_attestation_load_leaf_cert_der(uint8_t *out, size_t out_capacity, size_t *out_len) {
    (void)out;
    (void)out_capacity;
    if (out_len) {
        *out_len = 0;
    }
    return false;
}

bool zf_attestation_sign_input(const uint8_t *input, size_t input_len, uint8_t *out,
                               size_t out_capacity, size_t *out_len) {
    (void)input;
    (void)input_len;
    (void)out;
    (void)out_capacity;
    if (out_len) {
        *out_len = 0;
    }
    return false;
}

bool zf_attestation_sign_parts(const uint8_t *first, size_t first_len, const uint8_t *second,
                               size_t second_len, uint8_t *out, size_t out_capacity,
                               size_t *out_len) {
    (void)first;
    (void)first_len;
    (void)second;
    (void)second_len;
    (void)out;
    (void)out_capacity;
    if (out_len) {
        *out_len = 0;
    }
    return false;
}
#endif

bool zf_attestation_validate_consistency(void) {
    if (zf_attestation_consistency_cache >= 0) {
        return zf_attestation_consistency_cache != 0;
    }

    bool ok = sizeof(zf_attestation_aaguid) == ZF_AAGUID_LEN &&
              sizeof(zf_attestation_aaguid_string) == 37;
    zf_attestation_consistency_cache = ok ? 1 : 0;
    return ok;
}

void zf_attestation_reset_consistency_cache(void) {
    zf_attestation_consistency_cache = -1;
#if ZF_PACKED_ATTESTATION
    zf_attestation_leaf_cert_len_cache = 0;
#endif
}
