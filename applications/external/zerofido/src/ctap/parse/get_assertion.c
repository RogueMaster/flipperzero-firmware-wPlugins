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

#include "../extensions/hmac_secret.h"
#include "internal.h"

static uint8_t zf_parse_get_assertion_extensions(ZfCborCursor *cursor,
                                                 ZfGetAssertionRequest *request) {
    size_t pairs = 0;
    bool saw_hmac_secret = false;

    if (!zf_cbor_read_map_start(cursor, &pairs)) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    for (size_t i = 0; i < pairs; ++i) {
        const uint8_t *key = NULL;
        size_t key_size = 0;

        if (!zf_cbor_read_text_ptr(cursor, &key, &key_size)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }

        if (zf_ctap_classify_text_key(key, key_size) == ZfCtapTextKeyHmacSecret) {
            if (saw_hmac_secret) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            uint8_t status = zf_ctap_hmac_secret_parse_get_assertion_input(cursor, request);
            if (status != ZF_CTAP_SUCCESS) {
                return status;
            }
            saw_hmac_secret = true;
            continue;
        }

        if (!zf_cbor_skip(cursor)) {
            return ZF_CTAP_ERR_INVALID_CBOR;
        }
    }

    return ZF_CTAP_SUCCESS;
}

/*
 * Parses CTAP getAssertion. User presence defaults to true unless the options
 * map explicitly sets up=false; duplicate known keys, missing required fields,
 * malformed extensions, and trailing CBOR all fail before the handler sees the
 * request.
 */
uint8_t zf_ctap_parse_get_assertion(const uint8_t *data, size_t size,
                                    ZfGetAssertionRequest *request) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint16_t seen_keys = 0;
    ZfCredentialDescriptorList allow_list = request->allow_list;

    memset(request, 0, sizeof(*request));
    request->allow_list = allow_list;
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

        switch (key) {
        case 1:
            if (!zf_ctap_cbor_read_text_copy(&cursor, request->assertion.rp_id,
                                             sizeof(request->assertion.rp_id))) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            break;
        case 2: {
            size_t hash_len = 0;
            if (!zf_ctap_cbor_read_bytes_copy(&cursor, request->assertion.client_data_hash,
                                              sizeof(request->assertion.client_data_hash),
                                              &hash_len) ||
                hash_len != sizeof(request->assertion.client_data_hash)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->assertion.has_client_data_hash = true;
            break;
        }
        case 3: {
            uint8_t status = zf_ctap_parse_descriptor_array(&cursor, &request->allow_list);
            if (status != ZF_CTAP_SUCCESS) {
                return status;
            }
            break;
        }
        case 4: {
            uint8_t status = zf_parse_get_assertion_extensions(&cursor, request);
            if (status != ZF_CTAP_SUCCESS) {
                return status;
            }
            break;
        }
        case 5: {
            uint8_t status =
                zf_ctap_parse_options_map(&cursor, &request->up, &request->has_up, &request->uv,
                                          &request->has_uv, &request->rk, &request->has_rk);
            if (status != ZF_CTAP_SUCCESS) {
                return status;
            }
            break;
        }
        case 6:
            if (!zf_ctap_cbor_read_bytes_copy(&cursor, request->pin_auth, sizeof(request->pin_auth),
                                              &request->pin_auth_len)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_pin_auth = true;
            break;
        case 7:
            if (!zf_cbor_read_uint(&cursor, &request->pin_protocol)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_pin_protocol = true;
            break;
        default:
            if (!zf_cbor_skip(&cursor)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            break;
        }
    }

    if (request->assertion.rp_id[0] == '\0' || !request->assertion.has_client_data_hash) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    if (request->has_rk && request->rk) {
        return ZF_CTAP_ERR_UNSUPPORTED_OPTION;
    }
    if (cursor.ptr != cursor.end) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    return ZF_CTAP_SUCCESS;
}
