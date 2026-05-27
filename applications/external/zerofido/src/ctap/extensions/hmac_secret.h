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

#include "../../pin/protocol.h"
#include "../../zerofido_cbor.h"
#include "../../zerofido_crypto.h"
#include "../../zerofido_pin.h"
#include "../../zerofido_types.h"

typedef struct {
    uint8_t protocol_keys[ZF_PIN_PROTOCOL_KEYS_LEN];
    uint8_t salt_plain[ZF_HMAC_SECRET_SALT_MAX_LEN];
    uint8_t output_plain[ZF_HMAC_SECRET_SALT_MAX_LEN];
    uint8_t output_enc[ZF_HMAC_SECRET_OUTPUT_MAX_LEN];
    ZfHmacSha256Scratch hmac_scratch;
} ZfHmacSecretScratch;

uint8_t zf_ctap_hmac_secret_parse_get_assertion_input(ZfCborCursor *cursor,
                                                      ZfGetAssertionRequest *request);
bool zf_ctap_hmac_secret_parse_make_credential_request(ZfCborCursor *cursor, bool *requested);
bool zf_ctap_hmac_secret_encode_make_credential_output(ZfCborEncoder *enc, bool created);

/*
 * Implements the hmac-secret getAssertion extension. It decrypts one or two
 * salts, selects the per-credential secret based on UV state, and returns the
 * encrypted extension output.
 */
uint8_t zf_ctap_hmac_secret_build_extension(const ZfClientPinState *pin_state,
                                            const ZfAssertionRequestData *request,
                                            const ZfCredentialRecord *record, bool user_verified,
                                            ZfHmacSecretScratch *scratch, uint8_t *out,
                                            size_t out_capacity, size_t *out_len);
