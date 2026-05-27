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

#include "../../zerofido_cbor.h"

#if ZF_RELEASE_DIAGNOSTICS
static void zf_client_pin_response_diag_block(const char *label, const uint8_t *data, size_t len) {
    for (size_t off = 0; off < len; off += 16U) {
        uint8_t b[16] = {0};
        size_t chunk = len - off;
        if (chunk > sizeof(b)) {
            chunk = sizeof(b);
        }
        memcpy(b, data + off, chunk);
        FURI_LOG_I("ZeroFIDO:CTAP",
                   "%s[%u] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
                   "%02X %02X",
                   label, (unsigned)off, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9],
                   b[10], b[11], b[12], b[13], b[14], b[15]);
    }
}
#else
static void zf_client_pin_response_diag_block(const char *label, const uint8_t *data, size_t len) {
    UNUSED(label);
    UNUSED(data);
    UNUSED(len);
}
#endif

static uint8_t zf_client_pin_response_single_uint(uint64_t key, uint64_t value, uint8_t *out,
                                                  size_t out_capacity, size_t *out_len) {
    ZfCborEncoder enc;

    if (!zf_cbor_encoder_init(&enc, out, out_capacity)) {
        return ZF_CTAP_ERR_OTHER;
    }
    if (!(zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_uint(&enc, key) &&
          zf_cbor_encode_uint(&enc, value))) {
        return ZF_CTAP_ERR_OTHER;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_client_pin_response_single_bytes(uint64_t key, const uint8_t *value,
                                                   size_t value_len, uint8_t *out,
                                                   size_t out_capacity, size_t *out_len) {
    ZfCborEncoder enc;

    if (!value || value_len == 0 || !zf_cbor_encoder_init(&enc, out, out_capacity)) {
        return ZF_CTAP_ERR_OTHER;
    }
    if (!(zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_uint(&enc, key) &&
          zf_cbor_encode_bytes(&enc, value, value_len))) {
        return ZF_CTAP_ERR_OTHER;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    return ZF_CTAP_SUCCESS;
}

/* Encodes the CTAP ClientPIN getRetries response: map {3: pinRetries}. */
uint8_t zf_client_pin_response_retries(const ZfClientPinState *state, uint8_t *out,
                                       size_t out_capacity, size_t *out_len) {
    return zf_client_pin_response_single_uint(3, state->pin_retries, out, out_capacity, out_len);
}

/* Encodes the authenticator ephemeral P-256 key as a COSE_Key. */
uint8_t zf_client_pin_response_key_agreement(const ZfClientPinState *state, uint8_t *out,
                                             size_t out_capacity, size_t *out_len) {
    ZfCborEncoder enc;

    zf_client_pin_response_diag_block("cmd=CP-GA auth x", state->key_agreement.public_x,
                                      sizeof(state->key_agreement.public_x));
    zf_client_pin_response_diag_block("cmd=CP-GA auth y", state->key_agreement.public_y,
                                      sizeof(state->key_agreement.public_y));

    if (!zf_cbor_encoder_init(&enc, out, out_capacity)) {
        return ZF_CTAP_ERR_OTHER;
    }

    if (!(zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_uint(&enc, 1) &&
          zf_cbor_encode_p256_cose_key(
              &enc, -25, state->key_agreement.public_x, sizeof(state->key_agreement.public_x),
              state->key_agreement.public_y, sizeof(state->key_agreement.public_y)))) {
        return ZF_CTAP_ERR_OTHER;
    }

    *out_len = zf_cbor_encoder_size(&enc);
    return ZF_CTAP_SUCCESS;
}

/* Wraps an encrypted PIN/UV auth token in the ClientPIN response map. */
uint8_t zf_client_pin_response_token(const uint8_t *token, size_t token_len, uint8_t *out,
                                     size_t out_capacity, size_t *out_len) {
    return zf_client_pin_response_single_bytes(2, token, token_len, out, out_capacity, out_len);
}
