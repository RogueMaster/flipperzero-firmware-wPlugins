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

#include <stddef.h>
#include <stdint.h>

#include <storage/storage.h>

#include "../../zerofido_crypto.h"
#include "../internal.h"

typedef struct {
    ZfClientPinRequest request;
    ZfClientPinState state;
    uint8_t shared_secret[64];
    uint8_t current_pin_hash[ZF_PIN_HASH_LEN];
    uint8_t pin_hash_plain[32];
    uint8_t next_pin_token[ZF_PIN_TOKEN_LEN];
    uint8_t encrypted_token[ZF_PIN_ENCRYPTED_TOKEN_MAX_LEN];
    uint8_t new_pin_plain[ZF_PIN_NEW_PIN_BLOCK_MAX_LEN];
    ZfHmacSha256Scratch hmac_scratch;
} ZfClientPinCommandScratch;

_Static_assert(sizeof(ZfClientPinCommandScratch) <= ZF_COMMAND_SCRATCH_SIZE,
               "Client PIN scratch exceeds command arena");

/*
 * ClientPIN is split into parse, response, and operation modules. The scratch
 * object carries decrypted PIN blocks, token material, and HMAC state so command
 * code does not place secret arrays on the worker stack.
 */
uint8_t zf_client_pin_parse_request(const uint8_t *data, size_t size, ZfClientPinRequest *request);

uint8_t zf_client_pin_response_retries(const ZfClientPinState *state, uint8_t *out,
                                       size_t out_capacity, size_t *out_len);
uint8_t zf_client_pin_response_key_agreement(const ZfClientPinState *state, uint8_t *out,
                                             size_t out_capacity, size_t *out_len);
uint8_t zf_client_pin_response_token(const uint8_t *token, size_t token_len, uint8_t *out,
                                     size_t out_capacity, size_t *out_len);

uint8_t zf_client_pin_handle_set_pin(Storage *storage, ZfClientPinState *state,
                                     const ZfClientPinRequest *request,
                                     ZfClientPinCommandScratch *scratch, size_t *out_len);
uint8_t zf_client_pin_handle_change_pin(Storage *storage, ZfClientPinState *state,
                                        const ZfClientPinRequest *request,
                                        ZfClientPinCommandScratch *scratch, size_t *out_len);
uint8_t zf_client_pin_handle_get_pin_token(
    ZerofidoApp *app, Storage *storage, ZfClientPinState *state, const ZfClientPinRequest *request,
    ZfClientPinCommandScratch *scratch, bool permissions_mode, bool require_local_consent,
    ZfTransportSessionId session_id, uint8_t *out, size_t out_capacity, size_t *out_len);
