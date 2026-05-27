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

static bool zf_pin_validate_utf8(const uint8_t *pin, size_t pin_len, size_t *out_count);

enum { ZF_MAX_PIN_UTF8_BYTES = ZF_PIN_NEW_PIN_BLOCK_MAX_LEN - 1U };

bool zf_pin_new_pin_enc_length_is_valid(size_t length) {
    return length >= 64 && length <= ZF_PIN_NEW_PIN_BLOCK_MAX_LEN && (length % 16U) == 0U;
}

static uint8_t zf_pin_validate_plaintext_length(size_t pin_len) {
    if (pin_len < ZF_MIN_PIN_LENGTH || pin_len > ZF_MAX_PIN_UTF8_BYTES) {
        return ZF_CTAP_ERR_PIN_POLICY_VIOLATION;
    }
    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_pin_validate_plaintext_policy(const uint8_t *pin, size_t pin_len) {
    size_t pin_codepoints = 0;

    if (pin_len > ZF_MAX_PIN_UTF8_BYTES) {
        return ZF_CTAP_ERR_PIN_POLICY_VIOLATION;
    }
    if (!zf_pin_validate_utf8(pin, pin_len, &pin_codepoints)) {
        return ZF_CTAP_ERR_PIN_POLICY_VIOLATION;
    }

    return zf_pin_validate_plaintext_length(pin_codepoints);
}

/*
 * Minimal strict UTF-8 validator used for CTAP PIN policy. It counts Unicode
 * code points, rejects overlong encodings and surrogate code points, and does
 * not normalize or case-fold input.
 */
static bool zf_pin_validate_utf8(const uint8_t *pin, size_t pin_len, size_t *out_count) {
    size_t count = 0;

    for (size_t i = 0; i < pin_len;) {
        uint8_t lead = pin[i];

        if (lead <= 0x7F) {
            ++count;
            ++i;
            continue;
        }
        if (lead >= 0xC2 && lead <= 0xDF) {
            if (i + 1 >= pin_len || (pin[i + 1] & 0xC0U) != 0x80U) {
                return false;
            }
            ++count;
            i += 2;
            continue;
        }
        if (lead == 0xE0) {
            if (i + 2 >= pin_len || pin[i + 1] < 0xA0 || pin[i + 1] > 0xBF ||
                (pin[i + 2] & 0xC0U) != 0x80U) {
                return false;
            }
            ++count;
            i += 3;
            continue;
        }
        if ((lead >= 0xE1 && lead <= 0xEC) || (lead >= 0xEE && lead <= 0xEF)) {
            if (i + 2 >= pin_len || (pin[i + 1] & 0xC0U) != 0x80U ||
                (pin[i + 2] & 0xC0U) != 0x80U) {
                return false;
            }
            ++count;
            i += 3;
            continue;
        }
        if (lead == 0xED) {
            if (i + 2 >= pin_len || pin[i + 1] < 0x80 || pin[i + 1] > 0x9F ||
                (pin[i + 2] & 0xC0U) != 0x80U) {
                return false;
            }
            ++count;
            i += 3;
            continue;
        }
        if (lead == 0xF0) {
            if (i + 3 >= pin_len || pin[i + 1] < 0x90 || pin[i + 1] > 0xBF ||
                (pin[i + 2] & 0xC0U) != 0x80U || (pin[i + 3] & 0xC0U) != 0x80U) {
                return false;
            }
            ++count;
            i += 4;
            continue;
        }
        if (lead >= 0xF1 && lead <= 0xF3) {
            if (i + 3 >= pin_len || (pin[i + 1] & 0xC0U) != 0x80U ||
                (pin[i + 2] & 0xC0U) != 0x80U || (pin[i + 3] & 0xC0U) != 0x80U) {
                return false;
            }
            ++count;
            i += 4;
            continue;
        }
        if (lead == 0xF4) {
            if (i + 3 >= pin_len || pin[i + 1] < 0x80 || pin[i + 1] > 0x8F ||
                (pin[i + 2] & 0xC0U) != 0x80U || (pin[i + 3] & 0xC0U) != 0x80U) {
                return false;
            }
            ++count;
            i += 4;
            continue;
        }

        return false;
    }

    *out_count = count;
    return true;
}

static size_t zf_pin_unpadded_length(const uint8_t *data, size_t size) {
    size_t len = 0;
    while (len < size && data[len] != 0) {
        len++;
    }
    return len;
}

/*
 * CTAP encrypted PIN plaintext is a NUL-terminated UTF-8 PIN followed only by
 * zero padding. This extracts the unpadded PIN length; policy validation runs
 * separately on that slice.
 */
bool zf_pin_validate_plaintext_block(const uint8_t *data, size_t size, size_t *out_len) {
    size_t pin_len = zf_pin_unpadded_length(data, size);
    if (pin_len == size) {
        return false;
    }
    if (pin_len > ZF_MAX_PIN_UTF8_BYTES) {
        return false;
    }

    for (size_t i = pin_len; i < size; ++i) {
        if (data[i] != 0) {
            return false;
        }
    }

    *out_len = pin_len;
    return true;
}

uint8_t zf_pin_apply_plaintext(Storage *storage, ZfClientPinState *state, const uint8_t *pin,
                               size_t pin_len, bool require_unset) {
    uint8_t previous_hash[ZF_PIN_HASH_LEN];
    uint8_t pin_hash[32] = {0};
    uint8_t next_pin_token[ZF_PIN_TOKEN_LEN];
    uint8_t status = ZF_CTAP_SUCCESS;
    bool previous_pin_set = state->pin_set;
    uint8_t previous_pin_retries = state->pin_retries;
    uint8_t previous_pin_consecutive_mismatches = state->pin_consecutive_mismatches;
    bool previous_pin_auth_blocked = state->pin_auth_blocked;

    status = zf_pin_validate_plaintext_policy(pin, pin_len);
    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }
    if (require_unset && state->pin_set) {
        return ZF_CTAP_ERR_PIN_AUTH_INVALID;
    }
    zf_pin_refresh_pin_token(next_pin_token);

    zf_crypto_sha256(pin, pin_len, pin_hash);
    memcpy(previous_hash, state->pin_hash, sizeof(previous_hash));
    state->pin_set = true;
    state->pin_retries = ZF_PIN_RETRIES_MAX;
    zf_pin_clear_auth_block_state(state);
    memcpy(state->pin_hash, pin_hash, ZF_PIN_HASH_LEN);
    if (!zf_pin_persist_state(storage, state)) {
        memcpy(state->pin_hash, previous_hash, sizeof(previous_hash));
        state->pin_set = previous_pin_set;
        state->pin_retries = previous_pin_retries;
        state->pin_consecutive_mismatches = previous_pin_consecutive_mismatches;
        state->pin_auth_blocked = previous_pin_auth_blocked;
        zf_crypto_secure_zero(previous_hash, sizeof(previous_hash));
        zf_crypto_secure_zero(pin_hash, sizeof(pin_hash));
        zf_crypto_secure_zero(next_pin_token, sizeof(next_pin_token));
        return ZF_CTAP_ERR_OTHER;
    }

    zf_pin_invalidate_token_state(state);
    memcpy(state->pin_token, next_pin_token, sizeof(state->pin_token));
    zf_crypto_secure_zero(previous_hash, sizeof(previous_hash));
    zf_crypto_secure_zero(pin_hash, sizeof(pin_hash));
    zf_crypto_secure_zero(next_pin_token, sizeof(next_pin_token));
    return ZF_CTAP_SUCCESS;
}

uint8_t zerofido_pin_verify_plaintext(Storage *storage, ZfClientPinState *state, const char *pin) {
    uint8_t pin_hash[32] = {0};
    size_t pin_len = strlen(pin);
    uint8_t status = zf_pin_validate_plaintext_policy((const uint8_t *)pin, pin_len);
    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }

    zf_crypto_sha256((const uint8_t *)pin, pin_len, pin_hash);
    status = zf_pin_verify_hash(storage, state, pin_hash);
    zf_crypto_secure_zero(pin_hash, sizeof(pin_hash));
    return status;
}

uint8_t zerofido_pin_set_plaintext(Storage *storage, ZfClientPinState *state, const char *pin) {
    return zf_pin_apply_plaintext(storage, state, (const uint8_t *)pin, strlen(pin), true);
}

uint8_t zerofido_pin_replace_plaintext(Storage *storage, ZfClientPinState *state,
                                       const char *new_pin) {
    return zf_pin_apply_plaintext(storage, state, (const uint8_t *)new_pin, strlen(new_pin), false);
}
