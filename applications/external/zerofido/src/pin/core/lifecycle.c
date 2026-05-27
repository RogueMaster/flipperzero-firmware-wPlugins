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

#include "../../zerofido_pin.h"

#include <furi_hal_random.h>
#include <string.h>

#include "../../zerofido_app_i.h"
#include "../internal.h"
#include "../store/state_store.h"

static bool zf_pin_refresh_runtime_secrets(uint8_t pin_token[ZF_PIN_TOKEN_LEN],
                                           ZfP256KeyAgreementKey *key_agreement) {
    furi_hal_random_fill_buf(pin_token, ZF_PIN_TOKEN_LEN);
    memset(key_agreement, 0, sizeof(*key_agreement));
    return zf_crypto_generate_key_agreement_key(key_agreement);
}

ZfPinInitResult zerofido_pin_init_with_result(Storage *storage, ZfClientPinState *state) {
    ZfPinLoadStatus load_status = ZfPinLoadMissing;

    memset(state, 0, sizeof(*state));
    zf_pin_state_store_cleanup_temp(storage);
    state->pin_retries = ZF_PIN_RETRIES_MAX;
    load_status =
        zf_pin_state_store_load(storage, state->pin_hash, &state->pin_retries,
                                &state->pin_consecutive_mismatches, &state->pin_auth_blocked);
    if (load_status == ZfPinLoadInvalid) {
        zf_crypto_secure_zero(state->pin_hash, sizeof(state->pin_hash));
        return ZfPinInitInvalidPersistedState;
    }
    if (load_status == ZfPinLoadOk) {
        state->pin_set = true;
        /*
         * CTAP2 PIN_AUTH_BLOCKED is a temporary throttle that must clear when the authenticator
         * restarts, while the durable retry counter continues from persisted storage.
         */
        zf_pin_clear_auth_block_state(state);
    }
    if (!zf_pin_refresh_runtime_secrets(state->pin_token, &state->key_agreement)) {
        return ZfPinInitStorageError;
    }
    zf_pin_reset_token_metadata(state);
    return ZfPinInitOk;
}

bool zerofido_pin_init(Storage *storage, ZfClientPinState *state) {
    return zerofido_pin_init_with_result(storage, state) == ZfPinInitOk;
}

bool zerofido_pin_is_set(const ZfClientPinState *state) {
    return state->pin_set;
}

bool zerofido_pin_is_auth_blocked(const ZfClientPinState *state) {
    return state->pin_auth_blocked;
}

uint8_t zerofido_pin_get_retries(const ZfClientPinState *state) {
    return state->pin_retries;
}

bool zerofido_pin_clear(Storage *storage, ZfClientPinState *state) {
    uint8_t next_pin_token[ZF_PIN_TOKEN_LEN];
    ZfP256KeyAgreementKey next_key_agreement;

    if (!zf_pin_refresh_runtime_secrets(next_pin_token, &next_key_agreement)) {
        return false;
    }
    if (!zf_pin_state_store_clear(storage)) {
        zf_crypto_secure_zero(next_pin_token, sizeof(next_pin_token));
        zf_crypto_secure_zero(&next_key_agreement, sizeof(next_key_agreement));
        return false;
    }

    zf_crypto_secure_zero(state->pin_hash, sizeof(state->pin_hash));
    state->pin_set = false;
    state->pin_retries = ZF_PIN_RETRIES_MAX;
    zf_pin_clear_auth_block_state(state);
    zf_pin_invalidate_token_state(state);
    memcpy(state->pin_token, next_pin_token, sizeof(state->pin_token));
    state->key_agreement = next_key_agreement;
    zf_crypto_secure_zero(next_pin_token, sizeof(next_pin_token));
    zf_crypto_secure_zero(&next_key_agreement, sizeof(next_key_agreement));
    return true;
}
