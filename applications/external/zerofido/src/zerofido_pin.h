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

#include <storage/storage.h>

#include "zerofido_crypto.h"
#include "zerofido_types.h"

/*
 * Complete clientPIN runtime state. Only pin_hash, retry count, consecutive
 * mismatch count, and auth-block flag are durable. pin_token, token metadata,
 * and key_agreement are runtime secrets regenerated on init/reset and must not
 * be serialized.
 */
typedef struct {
    bool pin_set;
    uint8_t pin_hash[ZF_PIN_HASH_LEN];
    uint8_t pin_token[ZF_PIN_TOKEN_LEN];
    bool pin_token_active;
    uint32_t pin_token_issued_at;
    uint64_t pin_token_permissions;
    bool pin_token_permissions_scoped;
    bool pin_token_permissions_managed;
    bool pin_token_permissions_rp_id_set;
    uint8_t pin_token_permissions_rp_id_hash[32];
    uint8_t pin_retries;
    uint8_t pin_consecutive_mismatches;
    bool pin_auth_blocked;
    ZfP256KeyAgreementKey key_agreement;
} ZfClientPinState;

typedef struct ZerofidoApp ZerofidoApp;
typedef enum {
    ZfPinInitOk = 0,
    ZfPinInitInvalidPersistedState,
    ZfPinInitStorageError,
} ZfPinInitResult;

bool zerofido_pin_init(Storage *storage, ZfClientPinState *state);
ZfPinInitResult zerofido_pin_init_with_result(Storage *storage, ZfClientPinState *state);
bool zerofido_pin_is_set(const ZfClientPinState *state);
bool zerofido_pin_is_auth_blocked(const ZfClientPinState *state);
uint8_t zerofido_pin_get_retries(const ZfClientPinState *state);
uint8_t zerofido_pin_verify_plaintext(Storage *storage, ZfClientPinState *state, const char *pin);
uint8_t zerofido_pin_set_plaintext(Storage *storage, ZfClientPinState *state, const char *pin);
uint8_t zerofido_pin_replace_plaintext(Storage *storage, ZfClientPinState *state,
                                       const char *new_pin);
bool zerofido_pin_resume_auth_attempts(Storage *storage, ZfClientPinState *state);
bool zerofido_pin_clear(Storage *storage, ZfClientPinState *state);
uint8_t zerofido_pin_handle_command(ZerofidoApp *app, const uint8_t *request, size_t request_len,
                                    uint8_t *out, size_t out_capacity, size_t *out_len);
uint8_t zerofido_pin_handle_command_with_session(ZerofidoApp *app, ZfTransportSessionId session_id,
                                                 const uint8_t *request, size_t request_len,
                                                 uint8_t *out, size_t out_capacity,
                                                 size_t *out_len);
uint8_t zerofido_pin_require_auth(Storage *storage, ZfClientPinState *state, bool uv_requested,
                                  bool has_pin_auth,
                                  const uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN],
                                  const uint8_t *pin_auth, size_t pin_auth_len,
                                  bool has_pin_protocol, uint64_t pin_protocol, const char *rp_id,
                                  uint64_t required_permissions, bool *uv_verified);
