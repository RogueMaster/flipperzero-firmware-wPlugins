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

#include "../parse.h"

#include <string.h>

#include "internal.h"

static uint8_t zf_parse_make_credential_client_data_hash(ZfCborCursor *cursor,
                                                         ZfMakeCredentialRequest *request) {
    size_t hash_len = 0;
    if (!zf_ctap_cbor_read_bytes_copy(cursor, request->client_data_hash,
                                      sizeof(request->client_data_hash), &hash_len) ||
        hash_len != sizeof(request->client_data_hash)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    request->has_client_data_hash = true;
    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_parse_make_credential_rp(ZfCborCursor *cursor, ZfMakeCredentialRequest *request) {
    size_t rp_pairs = 0;
    bool saw_id = false;
    bool saw_name = false;
    bool saw_icon = false;

    if (!zf_cbor_read_map_start(cursor, &rp_pairs)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    for (size_t j = 0; j < rp_pairs; ++j) {
        const uint8_t *field = NULL;
        size_t field_size = 0;

        if (!zf_cbor_read_text_ptr(cursor, &field, &field_size)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }

        switch (zf_ctap_classify_text_key(field, field_size)) {
        case ZfCtapTextKeyName:
            if (saw_name || !zf_ctap_cbor_read_text_discard(cursor)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            saw_name = true;
            continue;

        case ZfCtapTextKeyIcon:
            if (saw_icon || !zf_ctap_cbor_read_text_discard(cursor)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            saw_icon = true;
            continue;

        case ZfCtapTextKeyId:
            if (saw_id) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            if (!zf_ctap_cbor_read_text_copy(cursor, request->rp_id, sizeof(request->rp_id))) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            saw_id = true;
            continue;

        default:
            if (!zf_cbor_skip(cursor)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            continue;
        }
    }

    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_parse_make_credential_user(ZfCborCursor *cursor,
                                             ZfMakeCredentialRequest *request) {
    size_t user_pairs = 0;
    bool saw_id = false;
    bool saw_name = false;
    bool saw_display_name = false;
    bool saw_icon = false;

    if (!zf_cbor_read_map_start(cursor, &user_pairs)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    for (size_t j = 0; j < user_pairs; ++j) {
        const uint8_t *field = NULL;
        size_t field_size = 0;

        if (!zf_cbor_read_text_ptr(cursor, &field, &field_size)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }

        switch (zf_ctap_classify_text_key(field, field_size)) {
        case ZfCtapTextKeyId:
            if (saw_id) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            if (!zf_ctap_cbor_read_bytes_copy(cursor, request->user_id, sizeof(request->user_id),
                                              &request->user_id_len)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_user_id = true;
            saw_id = true;
            continue;

        case ZfCtapTextKeyName:
            if (saw_name) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            if (!zf_ctap_cbor_read_text_copy(cursor, request->user_name,
                                             sizeof(request->user_name))) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            saw_name = true;
            continue;

        case ZfCtapTextKeyDisplayName:
            if (saw_display_name) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            if (!zf_ctap_cbor_read_text_copy(cursor, request->user_display_name,
                                             sizeof(request->user_display_name))) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            saw_display_name = true;
            continue;

        case ZfCtapTextKeyIcon:
            if (saw_icon || !zf_ctap_cbor_read_text_discard(cursor)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            saw_icon = true;
            continue;

        default:
            if (!zf_cbor_skip(cursor)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            break;
        }
    }

    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_parse_make_credential_pubkey_cred_params(ZfCborCursor *cursor,
                                                           ZfMakeCredentialRequest *request) {
    request->has_pubkey_cred_params = true;
    return zf_ctap_parse_pubkey_cred_params(cursor, &request->es256_supported);
}

static uint8_t zf_parse_make_credential_exclude_list(ZfCborCursor *cursor,
                                                     ZfMakeCredentialRequest *request) {
    uint8_t status = zf_ctap_parse_descriptor_array(cursor, &request->exclude_list);
    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }

    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_parse_make_credential_extensions(ZfCborCursor *cursor,
                                                   ZfMakeCredentialRequest *request) {
    return zf_ctap_parse_make_credential_extensions_map(cursor, &request->has_cred_protect,
                                                        &request->cred_protect,
                                                        &request->hmac_secret_requested);
}

static uint8_t zf_parse_make_credential_options(ZfCborCursor *cursor,
                                                ZfMakeCredentialRequest *request) {
    return zf_ctap_parse_options_map(cursor, &request->up, &request->has_up, &request->uv,
                                     &request->has_uv, &request->rk, &request->has_rk);
}

static uint8_t zf_parse_make_credential_pin_auth(ZfCborCursor *cursor,
                                                 ZfMakeCredentialRequest *request) {
    if (!zf_ctap_cbor_read_bytes_copy(cursor, request->pin_auth, sizeof(request->pin_auth),
                                      &request->pin_auth_len)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    request->has_pin_auth = true;
    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_parse_make_credential_pin_protocol(ZfCborCursor *cursor,
                                                     ZfMakeCredentialRequest *request) {
    if (!zf_cbor_read_uint(cursor, &request->pin_protocol)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    request->has_pin_protocol = true;
    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_parse_make_credential_attestation_formats(ZfCborCursor *cursor,
                                                            ZfMakeCredentialRequest *request) {
    size_t count = 0;

    if (!zf_cbor_read_array_start(cursor, &count)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    for (size_t i = 0; i < count; ++i) {
        const uint8_t *format = NULL;
        size_t format_size = 0;

        if (!zf_cbor_read_text_ptr(cursor, &format, &format_size) ||
            memchr(format, '\0', format_size) != NULL) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }
        if (request->has_attestation_format_preference) {
            continue;
        }
        if (format_size == 4U && memcmp(format, "none", 4U) == 0) {
            request->preferred_attestation_mode = ZfAttestationModeNone;
            request->has_attestation_format_preference = true;
#if ZF_PACKED_ATTESTATION
        } else if (format_size == 6U && memcmp(format, "packed", 6U) == 0) {
            request->preferred_attestation_mode = ZfAttestationModePacked;
            request->has_attestation_format_preference = true;
#endif
        }
    }

    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_parse_make_credential_field(ZfCborCursor *cursor,
                                              ZfMakeCredentialRequest *request, uint64_t key) {
    switch (key) {
    case 1:
        return zf_parse_make_credential_client_data_hash(cursor, request);
    case 2:
        return zf_parse_make_credential_rp(cursor, request);
    case 3:
        return zf_parse_make_credential_user(cursor, request);
    case 4:
        return zf_parse_make_credential_pubkey_cred_params(cursor, request);
    case 5:
        return zf_parse_make_credential_exclude_list(cursor, request);
    case 6:
        return zf_parse_make_credential_extensions(cursor, request);
    case 7:
        return zf_parse_make_credential_options(cursor, request);
    case 8:
        return zf_parse_make_credential_pin_auth(cursor, request);
    case 9:
        return zf_parse_make_credential_pin_protocol(cursor, request);
    case 11:
        return zf_parse_make_credential_attestation_formats(cursor, request);
    default:
        if (!zf_cbor_skip(cursor)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }
        return ZF_CTAP_SUCCESS;
    }
}

static uint8_t zf_validate_make_credential_request(const ZfCborCursor *cursor,
                                                   const ZfMakeCredentialRequest *request) {
    if (!request->has_client_data_hash || request->rp_id[0] == '\0' || !request->has_user_id) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    if (request->user_id_len == 0) {
        return ZF_CTAP_ERR_INVALID_PARAMETER;
    }
    if (!request->has_pubkey_cred_params) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    if (!request->es256_supported) {
        return ZF_CTAP_ERR_UNSUPPORTED_ALGORITHM;
    }
    if (request->has_up && !request->up) {
        return ZF_CTAP_ERR_INVALID_OPTION;
    }
    if (cursor->ptr != cursor->end) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    return ZF_CTAP_SUCCESS;
}

/*
 * Parses the CTAP makeCredential map into bounded internal storage. Known
 * numeric keys below 16 are duplicate-checked, required fields are enforced,
 * unsupported true options fail fast, and trailing CBOR bytes are rejected.
 */
uint8_t zf_ctap_parse_make_credential(const uint8_t *data, size_t size,
                                      ZfMakeCredentialRequest *request) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint16_t seen_keys = 0;
    ZfCredentialDescriptorList exclude_list = request->exclude_list;

    memset(request, 0, sizeof(*request));
    request->exclude_list = exclude_list;
    request->up = true;

    zf_cbor_cursor_init(&cursor, data, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }
        if (!zf_ctap_mark_seen_key(&seen_keys, key)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }

        uint8_t status = zf_parse_make_credential_field(&cursor, request, key);
        if (status != ZF_CTAP_SUCCESS) {
            return status;
        }
    }

    return zf_validate_make_credential_request(&cursor, request);
}
