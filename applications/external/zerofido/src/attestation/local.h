/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../zerofido_types.h"

#define ZF_LOCAL_ATTESTATION_EC_POINT_SIZE 65U

typedef struct {
    const char *data_dir;
    const char *assets_dir;
    const char *cert_file;
    const char *cert_temp_file;
    const char *key_file;
    const char *key_temp_file;
    const char *key_file_type;
    const uint8_t *subject_der;
    size_t subject_der_len;
    const uint8_t *extensions_der;
    size_t extensions_der_len;
    const uint8_t *identity;
    size_t identity_len;
} ZfLocalAttestationProfile;

/*
 * Local attestation profiles provide fixed DER subject/extension fragments and
 * target file paths. The implementation generates a self-signed P-256 leaf cert
 * and stores the matching private key encrypted under the device unique key.
 */
bool zf_local_attestation_ensure_assets(const ZfLocalAttestationProfile *profile);
bool zf_local_attestation_get_cert_size(const ZfLocalAttestationProfile *profile, size_t *out_len);
bool zf_local_attestation_load_cert(const ZfLocalAttestationProfile *profile, uint8_t *out,
                                    size_t out_capacity, size_t *out_len);
bool zf_local_attestation_load_private_key(const ZfLocalAttestationProfile *profile,
                                           uint8_t private_key[ZF_PRIVATE_KEY_LEN]);
bool zf_local_attestation_extract_cert_public_key(
    const uint8_t *cert, size_t cert_len, uint8_t public_key[ZF_LOCAL_ATTESTATION_EC_POINT_SIZE]);
bool zf_local_attestation_private_key_matches_cert(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                                   const uint8_t *cert, size_t cert_len,
                                                   const uint8_t *identity, size_t identity_len);
