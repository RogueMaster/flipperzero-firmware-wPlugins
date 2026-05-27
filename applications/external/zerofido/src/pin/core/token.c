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
#include "../../zerofido_usb_diagnostics.h"
#include "../internal.h"

#if ZF_USB_DIAGNOSTICS
static void zf_pin_usb_diag_hex(Storage *storage, const char *label, const uint8_t *data,
                                size_t len) {
    static const char hex_digits[] = "0123456789abcdef";
    char hex[65];
    size_t capped = len <= 32U ? len : 32U;

    if (!label || !data) {
        return;
    }
    for (size_t i = 0; i < capped; ++i) {
        hex[i * 2U] = hex_digits[data[i] >> 4U];
        hex[i * 2U + 1U] = hex_digits[data[i] & 0x0FU];
    }
    hex[capped * 2U] = '\0';
    zf_usb_diag_logf(storage, "pin auth %s len=%u %s", label, (unsigned)len, hex);
}

static void zf_pin_usb_diag_status(Storage *storage, const char *stage, uint8_t status) {
    if (!stage) {
        return;
    }
    zf_usb_diag_logf(storage, "pin auth %s status=%02X", stage, (unsigned)status);
}
#endif

static bool zf_pin_hash_rp_id(const char *rp_id, uint8_t out[32]) {
    if (!rp_id || rp_id[0] == '\0') {
        return false;
    }

    zf_crypto_sha256((const uint8_t *)rp_id, strlen(rp_id), out);
    return true;
}

void zf_pin_refresh_pin_token(uint8_t pin_token[ZF_PIN_TOKEN_LEN]) {
    furi_hal_random_fill_buf(pin_token, ZF_PIN_TOKEN_LEN);
}

void zf_pin_reset_token_metadata(ZfClientPinState *state) {
    state->pin_token_active = false;
    state->pin_token_issued_at = 0;
    state->pin_token_permissions = 0;
    state->pin_token_permissions_scoped = false;
    state->pin_token_permissions_managed = false;
    state->pin_token_permissions_rp_id_set = false;
    zf_crypto_secure_zero(state->pin_token_permissions_rp_id_hash,
                          sizeof(state->pin_token_permissions_rp_id_hash));
}

void zf_pin_invalidate_token_state(ZfClientPinState *state) {
    zf_crypto_secure_zero(state->pin_token, sizeof(state->pin_token));
    zf_pin_reset_token_metadata(state);
}

void zf_pin_note_pin_token_issued(ZfClientPinState *state) {
    state->pin_token_active = true;
    state->pin_token_issued_at = furi_get_tick();
}

void zf_pin_set_token_permissions(ZfClientPinState *state, uint64_t permissions,
                                  bool permission_scoped, bool permission_managed,
                                  const char *rp_id) {
    state->pin_token_permissions = permissions;
    state->pin_token_permissions_scoped = permission_scoped;
    state->pin_token_permissions_managed = permission_managed;
    state->pin_token_permissions_rp_id_set =
        zf_pin_hash_rp_id(rp_id, state->pin_token_permissions_rp_id_hash);
    if (!state->pin_token_permissions_rp_id_set) {
        zf_crypto_secure_zero(state->pin_token_permissions_rp_id_hash,
                              sizeof(state->pin_token_permissions_rp_id_hash));
    }
}

static bool zf_pin_token_rp_id_matches(const ZfClientPinState *state, const char *rp_id) {
    uint8_t rp_id_hash[32];
    bool matches = false;

    if (!zf_pin_hash_rp_id(rp_id, rp_id_hash)) {
        return false;
    }
    if (state->pin_token_permissions_rp_id_set) {
        matches = zf_crypto_constant_time_equal(
            state->pin_token_permissions_rp_id_hash, rp_id_hash,
            sizeof(state->pin_token_permissions_rp_id_hash));
    } else {
        matches = true;
    }
    zf_crypto_secure_zero(rp_id_hash, sizeof(rp_id_hash));
    return matches;
}

bool zf_pin_token_is_expired(const ZfClientPinState *state) {
    return (int32_t)(furi_get_tick() - state->pin_token_issued_at) >=
           (int32_t)ZF_PIN_TOKEN_TIMEOUT_MS;
}

static size_t zf_pin_uv_auth_param_len(uint64_t pin_protocol) {
    return pin_protocol == ZF_PIN_PROTOCOL_V2 ? ZF_PIN_AUTH_MAX_LEN : ZF_PIN_AUTH_LEN;
}

static void zf_pin_consume_up_tested_permissions(ZfClientPinState *state,
                                                 uint64_t required_permissions) {
    const uint64_t up_tested_permissions = ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA;

    if ((required_permissions & up_tested_permissions) == 0U) {
        return;
    }

    state->pin_token_permissions &= ~up_tested_permissions;
    if ((state->pin_token_permissions & up_tested_permissions) == 0U) {
        state->pin_token_permissions_scoped = false;
        state->pin_token_permissions_managed = false;
        state->pin_token_permissions_rp_id_set = false;
        zf_crypto_secure_zero(state->pin_token_permissions_rp_id_hash,
                              sizeof(state->pin_token_permissions_rp_id_hash));
    }
}

/*
 * Enforces makeCredential/getAssertion PIN/UV authorization. Verifies token
 * HMAC over clientDataHash using protocol-specific truncation, checks expiry,
 * permission mask, optional RP scoping, and consumes managed UP-tested
 * permissions after successful use.
 */
uint8_t zerofido_pin_require_auth(Storage *storage, ZfClientPinState *state, bool uv_requested,
                                  bool has_pin_auth,
                                  const uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN],
                                  const uint8_t *pin_auth, size_t pin_auth_len,
                                  bool has_pin_protocol, uint64_t pin_protocol, const char *rp_id,
                                  uint64_t required_permissions, bool *uv_verified) {
    uint8_t expected[32];

    *uv_verified = false;
#if ZF_USB_DIAGNOSTICS
    zf_usb_diag_logf(storage, "pin auth start uv=%u hpa=%u hpp=%u proto=%u len=%u",
                     uv_requested ? 1U : 0U, has_pin_auth ? 1U : 0U,
                     has_pin_protocol ? 1U : 0U, (unsigned)pin_protocol,
                     (unsigned)pin_auth_len);
    zf_usb_diag_logf(storage,
                     "pin auth state pin=%u token=%u block=%u perm=%02X req=%02X scoped=%u "
                     "managed=%u rpset=%u",
                     state->pin_set ? 1U : 0U, state->pin_token_active ? 1U : 0U,
                     state->pin_auth_blocked ? 1U : 0U,
                     (unsigned)state->pin_token_permissions, (unsigned)required_permissions,
                     state->pin_token_permissions_scoped ? 1U : 0U,
                     state->pin_token_permissions_managed ? 1U : 0U,
                     state->pin_token_permissions_rp_id_set ? 1U : 0U);
#endif
    if (has_pin_auth) {
        if (!has_pin_protocol) {
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "missing-proto", ZF_CTAP_ERR_MISSING_PARAMETER);
#endif
            return ZF_CTAP_ERR_MISSING_PARAMETER;
        }
        if (pin_protocol != ZF_PIN_PROTOCOL_V1 && pin_protocol != ZF_PIN_PROTOCOL_V2) {
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "bad-proto", ZF_CTAP_ERR_INVALID_PARAMETER);
#endif
            return ZF_CTAP_ERR_INVALID_PARAMETER;
        }
        if (pin_auth_len != zf_pin_uv_auth_param_len(pin_protocol)) {
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "bad-len", ZF_CTAP_ERR_PIN_AUTH_INVALID);
#endif
            return ZF_CTAP_ERR_PIN_AUTH_INVALID;
        }
        if (state->pin_auth_blocked) {
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "blocked", ZF_CTAP_ERR_PIN_AUTH_BLOCKED);
#endif
            return ZF_CTAP_ERR_PIN_AUTH_BLOCKED;
        }
        if (!state->pin_set) {
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "pin-not-set", ZF_CTAP_ERR_PIN_NOT_SET);
#endif
            return ZF_CTAP_ERR_PIN_NOT_SET;
        }
        if (!state->pin_token_active) {
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "token-inactive", ZF_CTAP_ERR_PIN_AUTH_INVALID);
#endif
            return ZF_CTAP_ERR_PIN_AUTH_INVALID;
        }
        if (zf_pin_token_is_expired(state)) {
            zf_pin_invalidate_token_state(state);
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "token-expired", ZF_CTAP_ERR_PIN_TOKEN_EXPIRED);
#endif
            return ZF_CTAP_ERR_PIN_TOKEN_EXPIRED;
        }

        if (!zf_crypto_hmac_sha256(state->pin_token, sizeof(state->pin_token), client_data_hash,
                                   ZF_CLIENT_DATA_HASH_LEN, expected)) {
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "hmac-failed", ZF_CTAP_ERR_OTHER);
#endif
            return ZF_CTAP_ERR_OTHER;
        }
#if ZF_USB_DIAGNOSTICS
        zf_pin_usb_diag_hex(storage, "cdh", client_data_hash, ZF_CLIENT_DATA_HASH_LEN);
        zf_pin_usb_diag_hex(storage, "got", pin_auth, pin_auth_len);
        zf_pin_usb_diag_hex(storage, "exp", expected, zf_pin_uv_auth_param_len(pin_protocol));
#endif
        if (!zf_crypto_constant_time_equal(expected, pin_auth,
                                           zf_pin_uv_auth_param_len(pin_protocol))) {
#if ZF_USB_DIAGNOSTICS
            uint8_t mismatch_status = zf_pin_note_pin_auth_mismatch(storage, state);
            zf_pin_usb_diag_status(storage, "mismatch", mismatch_status);
            zf_crypto_secure_zero(expected, sizeof(expected));
            return mismatch_status;
#else
            zf_crypto_secure_zero(expected, sizeof(expected));
            return zf_pin_note_pin_auth_mismatch(storage, state);
#endif
        }
        if ((state->pin_token_permissions & required_permissions) != required_permissions) {
            zf_crypto_secure_zero(expected, sizeof(expected));
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "perm", ZF_CTAP_ERR_PIN_AUTH_INVALID);
#endif
            return ZF_CTAP_ERR_PIN_AUTH_INVALID;
        }
        if (state->pin_token_permissions_scoped &&
            (required_permissions & (ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA)) != 0U) {
            if (!rp_id || rp_id[0] == '\0') {
                zf_crypto_secure_zero(expected, sizeof(expected));
#if ZF_USB_DIAGNOSTICS
                zf_pin_usb_diag_status(storage, "rp-missing", ZF_CTAP_ERR_PIN_AUTH_INVALID);
#endif
                return ZF_CTAP_ERR_PIN_AUTH_INVALID;
            }
            if (!zf_pin_token_rp_id_matches(state, rp_id)) {
                zf_crypto_secure_zero(expected, sizeof(expected));
#if ZF_USB_DIAGNOSTICS
                zf_pin_usb_diag_status(storage, "rp-mismatch", ZF_CTAP_ERR_PIN_AUTH_INVALID);
#endif
                return ZF_CTAP_ERR_PIN_AUTH_INVALID;
            }
            if (!state->pin_token_permissions_rp_id_set) {
                zf_pin_set_token_permissions(state, state->pin_token_permissions,
                                             state->pin_token_permissions_scoped,
                                             state->pin_token_permissions_managed, rp_id);
            }
        }

        uint8_t previous_pin_consecutive_mismatches = state->pin_consecutive_mismatches;
        bool previous_pin_auth_blocked = state->pin_auth_blocked;
        zf_pin_clear_auth_block_state(state);
        if (!zf_pin_persist_state(storage, state)) {
            state->pin_consecutive_mismatches = previous_pin_consecutive_mismatches;
            state->pin_auth_blocked = previous_pin_auth_blocked;
            zf_crypto_secure_zero(expected, sizeof(expected));
#if ZF_USB_DIAGNOSTICS
            zf_pin_usb_diag_status(storage, "persist", ZF_CTAP_ERR_OTHER);
#endif
            return ZF_CTAP_ERR_OTHER;
        }
        if (state->pin_token_permissions_managed) {
            zf_pin_consume_up_tested_permissions(state, required_permissions);
        }
        *uv_verified = true;
        zf_crypto_secure_zero(expected, sizeof(expected));
#if ZF_USB_DIAGNOSTICS
        zf_pin_usb_diag_status(storage, "ok", ZF_CTAP_SUCCESS);
#endif
        return ZF_CTAP_SUCCESS;
    }
    if (!state->pin_set) {
#if ZF_USB_DIAGNOSTICS
        zf_pin_usb_diag_status(storage, "no-pin-auth-pin-not-set",
                               uv_requested ? ZF_CTAP_ERR_PIN_NOT_SET : ZF_CTAP_SUCCESS);
#endif
        return uv_requested ? ZF_CTAP_ERR_PIN_NOT_SET : ZF_CTAP_SUCCESS;
    }
    if (!uv_requested) {
#if ZF_USB_DIAGNOSTICS
        zf_pin_usb_diag_status(storage, "no-pin-auth-no-uv", ZF_CTAP_SUCCESS);
#endif
        return ZF_CTAP_SUCCESS;
    }
#if ZF_USB_DIAGNOSTICS
    zf_pin_usb_diag_status(storage, "no-pin-auth-pin-required", ZF_CTAP_ERR_PIN_REQUIRED);
#endif
    return ZF_CTAP_ERR_PIN_REQUIRED;
}
