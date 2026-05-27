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

#include "internal.h"

#include <string.h>

#include "../../ctap/parse/internal.h"
#include "../../zerofido_cbor.h"

/* Only discovery commands can omit pinProtocol; crypto-bearing commands cannot. */
static bool zf_client_pin_subcommand_requires_pin_protocol(uint64_t subcommand) {
    switch (subcommand) {
    case ZF_CLIENT_PIN_SUBCMD_GET_RETRIES:
        return false;
    case ZF_CLIENT_PIN_SUBCMD_GET_KEY_AGREEMENT:
    case ZF_CLIENT_PIN_SUBCMD_SET_PIN:
    case ZF_CLIENT_PIN_SUBCMD_CHANGE_PIN:
    case ZF_CLIENT_PIN_SUBCMD_GET_PIN_TOKEN:
    case ZF_CLIENT_PIN_SUBCMD_GET_PIN_UV_AUTH_TOKEN_USING_PIN_WITH_PERMISSIONS:
        return true;
    default:
        return false;
    }
}

/*
 * Decodes the ClientPIN map into a flat request structure while preserving
 * which optional fields were present. Unknown keys are skipped as allowed by
 * CBOR extensibility, but duplicate known keys and trailing bytes fail closed.
 */
uint8_t zf_client_pin_parse_request(const uint8_t *data, size_t size, ZfClientPinRequest *request) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint16_t seen_keys = 0;

    memset(request, 0, sizeof(*request));
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
            if (!zf_cbor_read_uint(&cursor, &request->pin_protocol)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_pin_protocol = true;
            break;
        case 2:
            if (!zf_cbor_read_uint(&cursor, &request->subcommand)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_subcommand = true;
            break;
        case 3:
            request->has_key_agreement = zf_ctap_parse_cose_p256_key_agreement(
                &cursor, request->platform_x, request->platform_y);
            if (!request->has_key_agreement) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            break;
        case 4:
            if (!zf_ctap_cbor_read_bytes_copy(&cursor, request->pin_auth, sizeof(request->pin_auth),
                                              &request->pin_auth_len)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_pin_auth = true;
            break;
        case 5:
            if (!zf_ctap_cbor_read_bytes_copy(&cursor, request->new_pin_enc,
                                              sizeof(request->new_pin_enc),
                                              &request->new_pin_enc_len)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_new_pin_enc = true;
            break;
        case 6:
            if (!zf_ctap_cbor_read_bytes_copy(&cursor, request->pin_hash_enc,
                                              sizeof(request->pin_hash_enc),
                                              &request->pin_hash_enc_len)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_pin_hash_enc = true;
            break;
        case 9:
            if (!zf_cbor_read_uint(&cursor, &request->permissions)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_permissions = true;
            break;
        case 10:
            if (!zf_ctap_cbor_read_text_copy(&cursor, request->rp_id, sizeof(request->rp_id))) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            request->has_rp_id = true;
            break;
        default:
            if (!zf_cbor_skip(&cursor)) {
                return ZF_CTAP_ERR_INVALID_CBOR;
            }
            break;
        }
    }

    if (!request->has_subcommand) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    if (request->has_pin_protocol && request->pin_protocol != ZF_PIN_PROTOCOL_V1 &&
        request->pin_protocol != ZF_PIN_PROTOCOL_V2) {
        return ZF_CTAP_ERR_INVALID_PARAMETER;
    }
    if (zf_client_pin_subcommand_requires_pin_protocol(request->subcommand) &&
        !request->has_pin_protocol) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    if (cursor.ptr != cursor.end) {
        return ZF_CTAP_ERR_INVALID_CBOR;
    }

    return ZF_CTAP_SUCCESS;
}
