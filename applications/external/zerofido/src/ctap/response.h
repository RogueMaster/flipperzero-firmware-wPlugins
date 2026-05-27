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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "extensions/hmac_secret.h"
#include "../zerofido_runtime_config.h"
#include "../zerofido_types.h"

typedef struct {
    uint8_t cose[128];
    uint8_t extension_data[32];
    uint8_t signature[80];
} ZfMakeCredentialResponseScratch;

_Static_assert(sizeof(ZfMakeCredentialResponseScratch) <= 240U,
               "makeCredential response scratch must stay NFC-safe");

typedef struct {
    uint8_t auth_data[160];
    uint8_t extension_data[112];
    uint8_t sign_hash[32];
    uint8_t signature[80];
    ZfHmacSecretScratch hmac_secret;
} ZfAssertionResponseScratch;

/*
 * Response builders encode CTAP CBOR maps and authenticatorData. Scratch
 * structs carry temporary authData, COSE keys, signatures, certs, and extension
 * data so response generation stays off the worker stack.
 */
uint8_t zf_ctap_build_get_info_response(const ZfResolvedCapabilities *capabilities,
                                        bool client_pin_set, uint8_t *out, size_t out_capacity,
                                        size_t *out_len);
uint8_t zf_ctap_build_packed_make_credential_response_with_scratch(
    ZfMakeCredentialResponseScratch *scratch, const char *rp_id, const ZfCredentialRecord *record,
    const uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN], bool user_verified,
    bool include_cred_protect, bool include_hmac_secret, uint8_t *out, size_t out_capacity,
    size_t *out_len);
uint8_t zf_ctap_build_none_make_credential_response_with_scratch(
    ZfMakeCredentialResponseScratch *scratch, const char *rp_id, const ZfCredentialRecord *record,
    bool user_verified, bool include_cred_protect, bool include_hmac_secret, uint8_t *out,
    size_t out_capacity, size_t *out_len);
uint8_t zf_ctap_build_assertion_response_with_scratch(
    ZfAssertionResponseScratch *scratch, const ZfAssertionRequestData *request,
    const ZfCredentialRecord *record, bool user_present, bool user_verified, uint32_t sign_count,
    bool include_user_details, bool include_count, size_t match_count, bool include_user_selected,
    bool user_selected, const uint8_t *extension_data, size_t extension_data_len, uint8_t *out,
    size_t out_capacity, size_t *out_len);
