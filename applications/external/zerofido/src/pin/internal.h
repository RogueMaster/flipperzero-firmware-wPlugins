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

#include "../zerofido_app_i.h"
#include "../zerofido_pin.h"

enum {
    ZF_CLIENT_PIN_SUBCMD_GET_RETRIES = 0x01,
    ZF_CLIENT_PIN_SUBCMD_GET_KEY_AGREEMENT = 0x02,
    ZF_CLIENT_PIN_SUBCMD_SET_PIN = 0x03,
    ZF_CLIENT_PIN_SUBCMD_CHANGE_PIN = 0x04,
    ZF_CLIENT_PIN_SUBCMD_GET_PIN_TOKEN = 0x05,
    ZF_CLIENT_PIN_SUBCMD_GET_PIN_UV_AUTH_TOKEN_USING_UV_WITH_PERMISSIONS = 0x06,
    ZF_CLIENT_PIN_SUBCMD_GET_PIN_UV_AUTH_TOKEN_USING_PIN_WITH_PERMISSIONS = 0x09,
};

/*
 * Parsed ClientPIN request. Boolean presence flags are kept separate from
 * values because zero is a valid protocol value for several fields.
 */
typedef struct {
    uint64_t pin_protocol;
    uint64_t subcommand;
    uint64_t permissions;
    size_t pin_auth_len;
    size_t new_pin_enc_len;
    size_t pin_hash_enc_len;
    bool has_pin_protocol;
    bool has_subcommand;
    bool has_key_agreement;
    bool has_pin_auth;
    bool has_new_pin_enc;
    bool has_pin_hash_enc;
    bool has_permissions;
    bool has_rp_id;
    uint8_t pin_auth[ZF_PIN_AUTH_MAX_LEN];
    uint8_t platform_x[ZF_PUBLIC_KEY_LEN];
    uint8_t platform_y[ZF_PUBLIC_KEY_LEN];
    uint8_t pin_hash_enc[ZF_PIN_ENCRYPTED_HASH_MAX_LEN];
    uint8_t new_pin_enc[ZF_PIN_ENCRYPTED_NEW_PIN_MAX_LEN];
    char rp_id[ZF_MAX_RP_ID_LEN];
} ZfClientPinRequest;

/* Shared PIN state helpers used by ClientPIN operation handlers and CTAP UV checks. */
bool zf_pin_new_pin_enc_length_is_valid(size_t length);
void zf_pin_refresh_pin_token(uint8_t pin_token[ZF_PIN_TOKEN_LEN]);
void zf_pin_reset_token_metadata(ZfClientPinState *state);
void zf_pin_invalidate_token_state(ZfClientPinState *state);
void zf_pin_note_pin_token_issued(ZfClientPinState *state);
void zf_pin_set_token_permissions(ZfClientPinState *state, uint64_t permissions,
                                  bool permission_scoped, bool permission_managed,
                                  const char *rp_id);
bool zf_pin_token_is_expired(const ZfClientPinState *state);
void zf_pin_clear_auth_block_state(ZfClientPinState *state);
bool zf_pin_persist_state(Storage *storage, const ZfClientPinState *state);
uint8_t zf_pin_note_pin_auth_mismatch(Storage *storage, ZfClientPinState *state);
bool zf_pin_validate_plaintext_block(const uint8_t *data, size_t size, size_t *out_len);
uint8_t zf_pin_apply_plaintext(Storage *storage, ZfClientPinState *state, const uint8_t *pin,
                               size_t pin_len, bool require_unset);
uint8_t zf_pin_auth_failure(Storage *storage, ZfClientPinState *state);
uint8_t zf_pin_auth_success(Storage *storage, ZfClientPinState *state);
uint8_t zf_pin_verify_hash(Storage *storage, ZfClientPinState *state,
                           const uint8_t pin_hash[ZF_PIN_HASH_LEN]);
