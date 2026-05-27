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

#include <string.h>

#include "../../zerofido_app_i.h"
#include "../internal.h"
#include "../store/state_store.h"

bool zf_pin_persist_state(Storage *storage, const ZfClientPinState *state) {
    return zf_pin_state_store_persist(storage, state);
}

static bool zf_pin_refresh_key_agreement(ZfP256KeyAgreementKey *key_agreement) {
    memset(key_agreement, 0, sizeof(*key_agreement));
    return zf_crypto_generate_key_agreement_key(key_agreement);
}

void zf_pin_clear_auth_block_state(ZfClientPinState *state) {
    state->pin_consecutive_mismatches = 0;
    state->pin_auth_blocked = false;
}

static void zf_pin_force_runtime_block(ZfClientPinState *state) {
    state->pin_retries = 0;
    state->pin_consecutive_mismatches = 3;
    state->pin_auth_blocked = true;
    zf_pin_invalidate_token_state(state);
}

/*
 * Records a pinUvAuthParam mismatch. This does not decrement pin_retries, but
 * three consecutive mismatches enter PIN_AUTH_BLOCKED until explicit local
 * resume/restart handling clears the temporary throttle.
 */
uint8_t zf_pin_note_pin_auth_mismatch(Storage *storage, ZfClientPinState *state) {
    if (state->pin_consecutive_mismatches < UINT8_MAX) {
        state->pin_consecutive_mismatches++;
    }
    if (state->pin_consecutive_mismatches >= 3) {
        state->pin_auth_blocked = true;
    }

    /*
     * Fail closed when storage is degraded: keep the stricter in-memory mismatch and block
     * state even if persisting it does not succeed.
     */
    if (!zf_pin_persist_state(storage, state) && !zf_pin_state_store_fail_closed(storage, state)) {
        zf_pin_force_runtime_block(state);
    }

    return state->pin_auth_blocked ? ZF_CTAP_ERR_PIN_AUTH_BLOCKED : ZF_CTAP_ERR_PIN_AUTH_INVALID;
}

/*
 * Records a failed PIN-hash verification. This consumes one durable retry,
 * refreshes the key-agreement key per CTAP expectations, and fail-closes if the
 * stricter state cannot be persisted.
 */
uint8_t zf_pin_auth_failure(Storage *storage, ZfClientPinState *state) {
    ZfP256KeyAgreementKey next_key_agreement;

    if (state->pin_auth_blocked) {
        return ZF_CTAP_ERR_PIN_AUTH_BLOCKED;
    }
    if (!zf_pin_refresh_key_agreement(&next_key_agreement)) {
        return ZF_CTAP_ERR_OTHER;
    }
    state->key_agreement = next_key_agreement;
    zf_crypto_secure_zero(&next_key_agreement, sizeof(next_key_agreement));
    if (state->pin_retries > 0) {
        state->pin_retries--;
    }
    if (state->pin_consecutive_mismatches < UINT8_MAX) {
        state->pin_consecutive_mismatches++;
    }
    if (state->pin_consecutive_mismatches >= 3) {
        state->pin_auth_blocked = true;
    }

    /*
     * Preserve the stricter in-memory retry, block, and key-agreement state even if persistence
     * fails so degraded storage cannot reopen brute-force attempts during the running session.
     */
    if (!zf_pin_persist_state(storage, state) && !zf_pin_state_store_fail_closed(storage, state)) {
        zf_pin_force_runtime_block(state);
    }
    if (state->pin_retries == 0) {
        return ZF_CTAP_ERR_PIN_BLOCKED;
    }

    return state->pin_auth_blocked ? ZF_CTAP_ERR_PIN_AUTH_BLOCKED : ZF_CTAP_ERR_PIN_INVALID;
}

uint8_t zf_pin_auth_success(Storage *storage, ZfClientPinState *state) {
    uint8_t previous_pin_retries = state->pin_retries;
    uint8_t previous_pin_consecutive_mismatches = state->pin_consecutive_mismatches;
    bool previous_pin_auth_blocked = state->pin_auth_blocked;

    state->pin_retries = ZF_PIN_RETRIES_MAX;
    zf_pin_clear_auth_block_state(state);
    if (zf_pin_persist_state(storage, state)) {
        return ZF_CTAP_SUCCESS;
    }

    state->pin_retries = previous_pin_retries;
    state->pin_consecutive_mismatches = previous_pin_consecutive_mismatches;
    state->pin_auth_blocked = previous_pin_auth_blocked;
    return ZF_CTAP_ERR_OTHER;
}

uint8_t zf_pin_verify_hash(Storage *storage, ZfClientPinState *state,
                           const uint8_t pin_hash[ZF_PIN_HASH_LEN]) {
    if (!state->pin_set) {
        return ZF_CTAP_ERR_PIN_NOT_SET;
    }
    if (state->pin_auth_blocked) {
        return ZF_CTAP_ERR_PIN_AUTH_BLOCKED;
    }
    if (state->pin_retries == 0) {
        return ZF_CTAP_ERR_PIN_BLOCKED;
    }
    if (!zf_crypto_constant_time_equal(pin_hash, state->pin_hash, ZF_PIN_HASH_LEN)) {
        return zf_pin_auth_failure(storage, state);
    }

    return zf_pin_auth_success(storage, state);
}

bool zerofido_pin_resume_auth_attempts(Storage *storage, ZfClientPinState *state) {
    uint8_t previous_pin_consecutive_mismatches = state->pin_consecutive_mismatches;
    bool previous_pin_auth_blocked = state->pin_auth_blocked;

    if (!state->pin_set) {
        return false;
    }
    if (!state->pin_auth_blocked && state->pin_consecutive_mismatches == 0) {
        return true;
    }

    /*
     * Stock external apps do not have an app-owned retained power-session primitive.
     * ZeroFIDO therefore persists PIN_AUTH_BLOCKED and clears it only through this
     * explicit local unblock ceremony instead of depending on fragile hidden firmware
     * state. This is deliberate, even though it is not literal CTAP power-cycle semantics.
     */
    zf_pin_clear_auth_block_state(state);
    if (zf_pin_persist_state(storage, state)) {
        return true;
    }

    state->pin_consecutive_mismatches = previous_pin_consecutive_mismatches;
    state->pin_auth_blocked = previous_pin_auth_blocked;
    return false;
}
