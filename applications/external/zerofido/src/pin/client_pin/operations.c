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

#include "../../zerofido_ui.h"
#include "../protocol.h"

#if ZF_RELEASE_DIAGNOSTICS
#define ZF_PIN_OP_DIAG(...) FURI_LOG_I("ZeroFIDO:CTAP", __VA_ARGS__)
static void zf_client_pin_diag_secret_block(const char *label, const uint8_t *data, size_t len) {
    UNUSED(data);
    FURI_LOG_I("ZeroFIDO:CTAP", "%s len=%u redacted", label, (unsigned)len);
}
#else
#define ZF_PIN_OP_DIAG(...)                                                                        \
    do {                                                                                           \
    } while (false)
static void zf_client_pin_diag_secret_block(const char *label, const uint8_t *data, size_t len) {
    UNUSED(label);
    UNUSED(data);
    UNUSED(len);
}
#endif

static void zf_client_pin_diag_key_exchange(const ZfClientPinState *state,
                                            const ZfClientPinRequest *request,
                                            uint8_t keys[ZF_PIN_PROTOCOL_KEYS_LEN]) {
    if (!state || !request || !keys) {
        return;
    }
    ZF_PIN_OP_DIAG("cmd=CP key proto=%lu", (unsigned long)request->pin_protocol);
    zf_client_pin_diag_secret_block("cmd=CP key auth x", state->key_agreement.public_x,
                                    ZF_PUBLIC_KEY_LEN);
    zf_client_pin_diag_secret_block("cmd=CP key auth y", state->key_agreement.public_y,
                                    ZF_PUBLIC_KEY_LEN);
    zf_client_pin_diag_secret_block("cmd=CP key platform x", request->platform_x,
                                    ZF_PUBLIC_KEY_LEN);
    zf_client_pin_diag_secret_block("cmd=CP key platform y", request->platform_y,
                                    ZF_PUBLIC_KEY_LEN);
    zf_client_pin_diag_secret_block("cmd=CP key hmac", zf_pin_protocol_hmac_key(keys),
                                    ZF_PUBLIC_KEY_LEN);
    zf_client_pin_diag_secret_block("cmd=CP key aes", zf_pin_protocol_aes_key(keys),
                                    ZF_PUBLIC_KEY_LEN);
}

static size_t zf_pin_protocol_encrypted_pin_hash_len(uint64_t pin_protocol) {
    return pin_protocol == ZF_PIN_PROTOCOL_V2 ? ZF_PIN_ENCRYPTED_HASH_MAX_LEN : ZF_PIN_HASH_LEN;
}

static bool zf_client_pin_new_pin_ciphertext_len_is_valid(uint64_t pin_protocol,
                                                          size_t ciphertext_len,
                                                          size_t *plaintext_len) {
    size_t len = ciphertext_len;
    /* Protocol v2 prefixes a random IV; PIN block policy applies after removal. */
    if (pin_protocol == ZF_PIN_PROTOCOL_V2) {
        if (ciphertext_len <= ZF_PIN_PROTOCOL2_IV_LEN) {
            return false;
        }
        len = ciphertext_len - ZF_PIN_PROTOCOL2_IV_LEN;
    }

    if (!zf_pin_new_pin_enc_length_is_valid(len)) {
        return false;
    }
    *plaintext_len = len;
    return true;
}

static uint8_t zf_client_pin_status_from_interaction_state(ZfApprovalState state) {
    switch (state) {
    case ZfApprovalTimedOut:
        return ZF_CTAP_ERR_USER_ACTION_TIMEOUT;
    case ZfApprovalCanceled:
        return ZF_CTAP_ERR_KEEPALIVE_CANCEL;
    case ZfApprovalDenied:
    case ZfApprovalIdle:
    case ZfApprovalPending:
    case ZfApprovalApproved:
    default:
        return ZF_CTAP_ERR_OPERATION_DENIED;
    }
}

static uint8_t zf_client_pin_request_permission_token_consent(
    ZerofidoApp *app, ZfClientPinState *state, const ZfClientPinRequest *request,
    const uint8_t pin_hash_plain[ZF_PIN_HASH_LEN], ZfTransportSessionId session_id) {
    bool approved = false;
    bool held_maintenance = false;
    uint8_t status = ZF_CTAP_SUCCESS;
    const char *target_id = request->has_rp_id ? request->rp_id : "PIN token";

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->pin_state = *state;
    held_maintenance = app->maintenance_busy;
    if (held_maintenance) {
        app->maintenance_busy = false;
    }
    furi_mutex_release(app->ui_mutex);

    if (!zerofido_ui_request_approval(app, ZfUiProtocolFido2, "Authorize", target_id,
                                      "Approve PIN use", session_id, &approved)) {
        status = ZF_CTAP_ERR_USER_ACTION_TIMEOUT;
    } else if (!approved) {
        status =
            zf_client_pin_status_from_interaction_state(zerofido_ui_get_interaction_state(app));
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (held_maintenance) {
        if (app->maintenance_busy) {
            furi_mutex_release(app->ui_mutex);
            return status == ZF_CTAP_SUCCESS ? ZF_CTAP_ERR_NOT_ALLOWED : status;
        }
        app->maintenance_busy = true;
    }
    *state = app->pin_state;
    furi_mutex_release(app->ui_mutex);

    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }
    if (!state->pin_set ||
        !zf_crypto_constant_time_equal(pin_hash_plain, state->pin_hash, ZF_PIN_HASH_LEN)) {
        return ZF_CTAP_ERR_PIN_AUTH_INVALID;
    }

    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_client_pin_require_existing_pin(const ZfClientPinState *state) {
    if (!state->pin_set) {
        return ZF_CTAP_ERR_PIN_NOT_SET;
    }
    if (state->pin_auth_blocked) {
        return ZF_CTAP_ERR_PIN_AUTH_BLOCKED;
    }
    if (state->pin_retries == 0U) {
        return ZF_CTAP_ERR_PIN_BLOCKED;
    }
    return ZF_CTAP_SUCCESS;
}

static uint8_t zf_client_pin_decrypt_new_pin(const ZfClientPinRequest *request,
                                             uint8_t *protocol_keys, uint8_t *new_pin_plain,
                                             size_t new_pin_plain_len, size_t *pin_len) {
    size_t decrypted_len = 0;

    if (!zf_pin_protocol_decrypt(request->pin_protocol, protocol_keys, request->new_pin_enc,
                                 request->new_pin_enc_len, new_pin_plain, &decrypted_len) ||
        decrypted_len != new_pin_plain_len) {
        ZF_PIN_OP_DIAG("cmd=CP newpin decrypt fail dec=%u exp=%u", (unsigned)decrypted_len,
                       (unsigned)new_pin_plain_len);
        return ZF_CTAP_ERR_PIN_AUTH_INVALID;
    }
    zf_client_pin_diag_secret_block("cmd=CP newpin enc", request->new_pin_enc,
                                    request->new_pin_enc_len);
    zf_client_pin_diag_secret_block("cmd=CP newpin plain", new_pin_plain, new_pin_plain_len);
    if (!zf_pin_validate_plaintext_block(new_pin_plain, new_pin_plain_len, pin_len)) {
        ZF_PIN_OP_DIAG("cmd=CP newpin block invalid len=%u", (unsigned)new_pin_plain_len);
        return ZF_CTAP_ERR_PIN_POLICY_VIOLATION;
    }
    ZF_PIN_OP_DIAG("cmd=CP newpin block ok pin_len=%u", (unsigned)*pin_len);
    return ZF_CTAP_SUCCESS;
}

uint8_t zf_client_pin_handle_set_pin(Storage *storage, ZfClientPinState *state,
                                     const ZfClientPinRequest *request,
                                     ZfClientPinCommandScratch *scratch, size_t *out_len) {
    uint8_t *protocol_keys = scratch->shared_secret;
    uint8_t *new_pin_plain = scratch->new_pin_plain;
    size_t new_pin_plain_len = 0;
    size_t pin_len = 0;
    uint8_t status = ZF_CTAP_SUCCESS;

    if (state->pin_set) {
        status = ZF_CTAP_ERR_PIN_AUTH_INVALID;
        goto cleanup;
    }
    if (!request->has_key_agreement || !request->has_new_pin_enc || !request->has_pin_auth) {
        status = ZF_CTAP_ERR_MISSING_PARAMETER;
        goto cleanup;
    }
    ZF_PIN_OP_DIAG("cmd=CP-SP params");
    ZF_PIN_OP_DIAG("cmd=CP-SP proto=%lu new=%u auth=%u", (unsigned long)request->pin_protocol,
                   (unsigned)request->new_pin_enc_len, (unsigned)request->pin_auth_len);
    if (!zf_pin_protocol_supported(request->pin_protocol) ||
        !zf_client_pin_new_pin_ciphertext_len_is_valid(
            request->pin_protocol, request->new_pin_enc_len, &new_pin_plain_len)) {
        status = ZF_CTAP_ERR_INVALID_PARAMETER;
        goto cleanup;
    }
    ZF_PIN_OP_DIAG("cmd=CP-SP derive");
    if (!zf_pin_protocol_derive_keys(state, request->pin_protocol, request->platform_x,
                                     request->platform_y, protocol_keys)) {
        status = ZF_CTAP_ERR_INVALID_PARAMETER;
        goto cleanup;
    }
    zf_client_pin_diag_key_exchange(state, request, protocol_keys);
    ZF_PIN_OP_DIAG("cmd=CP-SP auth");
    if (!zf_pin_protocol_hmac_matches(&scratch->hmac_scratch, request->pin_protocol,
                                      zf_pin_protocol_hmac_key(protocol_keys), request->new_pin_enc,
                                      request->new_pin_enc_len, NULL, 0, request->pin_auth,
                                      request->pin_auth_len)) {
        status = ZF_CTAP_ERR_PIN_AUTH_INVALID;
        goto cleanup;
    }
    ZF_PIN_OP_DIAG("cmd=CP-SP decrypt");
    status = zf_client_pin_decrypt_new_pin(request, protocol_keys, new_pin_plain, new_pin_plain_len,
                                           &pin_len);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    ZF_PIN_OP_DIAG("cmd=CP-SP persist");
    status = zf_pin_apply_plaintext(storage, state, new_pin_plain, pin_len, true);
    if (status != ZF_CTAP_SUCCESS) {
        ZF_PIN_OP_DIAG("cmd=CP-SP policy status=%02X pin_len=%u", status, (unsigned)pin_len);
        goto cleanup;
    }

    *out_len = 0;
    status = ZF_CTAP_SUCCESS;

cleanup:
    zf_crypto_secure_zero(protocol_keys, sizeof(scratch->shared_secret));
    zf_crypto_secure_zero(new_pin_plain, sizeof(scratch->new_pin_plain));
    return status;
}

uint8_t zf_client_pin_handle_change_pin(Storage *storage, ZfClientPinState *state,
                                        const ZfClientPinRequest *request,
                                        ZfClientPinCommandScratch *scratch, size_t *out_len) {
    uint8_t *protocol_keys = scratch->shared_secret;
    uint8_t *current_pin_hash = scratch->current_pin_hash;
    uint8_t *new_pin_plain = scratch->new_pin_plain;
    size_t new_pin_plain_len = 0;
    size_t decrypted_len = 0;
    size_t pin_len = 0;
    uint8_t status = ZF_CTAP_SUCCESS;

    status = zf_client_pin_require_existing_pin(state);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    if (!request->has_key_agreement || !request->has_new_pin_enc || !request->has_pin_hash_enc ||
        !request->has_pin_auth) {
        status = ZF_CTAP_ERR_MISSING_PARAMETER;
        goto cleanup;
    }
    ZF_PIN_OP_DIAG("cmd=CP-CH proto=%lu new=%u hash=%u auth=%u",
                   (unsigned long)request->pin_protocol, (unsigned)request->new_pin_enc_len,
                   (unsigned)request->pin_hash_enc_len, (unsigned)request->pin_auth_len);
    if (!zf_pin_protocol_supported(request->pin_protocol) ||
        !zf_client_pin_new_pin_ciphertext_len_is_valid(
            request->pin_protocol, request->new_pin_enc_len, &new_pin_plain_len) ||
        request->pin_hash_enc_len !=
            zf_pin_protocol_encrypted_pin_hash_len(request->pin_protocol)) {
        status = ZF_CTAP_ERR_INVALID_PARAMETER;
        goto cleanup;
    }
    if (!zf_pin_protocol_derive_keys(state, request->pin_protocol, request->platform_x,
                                     request->platform_y, protocol_keys)) {
        status = ZF_CTAP_ERR_INVALID_PARAMETER;
        goto cleanup;
    }
    zf_client_pin_diag_key_exchange(state, request, protocol_keys);
    zf_client_pin_diag_secret_block("cmd=CP pin hash enc", request->pin_hash_enc,
                                    request->pin_hash_enc_len);
    if (!zf_pin_protocol_hmac_matches(
            &scratch->hmac_scratch, request->pin_protocol, zf_pin_protocol_hmac_key(protocol_keys),
            request->new_pin_enc, request->new_pin_enc_len, request->pin_hash_enc,
            request->pin_hash_enc_len, request->pin_auth, request->pin_auth_len)) {
        status = ZF_CTAP_ERR_PIN_AUTH_INVALID;
        goto cleanup;
    }
    if (!zf_pin_protocol_decrypt(request->pin_protocol, protocol_keys, request->pin_hash_enc,
                                 request->pin_hash_enc_len, current_pin_hash, &decrypted_len) ||
        decrypted_len != ZF_PIN_HASH_LEN) {
        status = zf_pin_auth_failure(storage, state);
        goto cleanup;
    }
    status = zf_pin_verify_hash(storage, state, current_pin_hash);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }

    status = zf_client_pin_decrypt_new_pin(request, protocol_keys, new_pin_plain, new_pin_plain_len,
                                           &pin_len);
    if (status != ZF_CTAP_SUCCESS) {
        goto cleanup;
    }
    status = zf_pin_apply_plaintext(storage, state, new_pin_plain, pin_len, false);
    if (status != ZF_CTAP_SUCCESS) {
        ZF_PIN_OP_DIAG("cmd=CP-CH policy status=%02X pin_len=%u", status, (unsigned)pin_len);
        goto cleanup;
    }

    *out_len = 0;
    status = ZF_CTAP_SUCCESS;

cleanup:
    zf_crypto_secure_zero(protocol_keys, sizeof(scratch->shared_secret));
    zf_crypto_secure_zero(current_pin_hash, sizeof(scratch->current_pin_hash));
    zf_crypto_secure_zero(new_pin_plain, sizeof(scratch->new_pin_plain));
    return status;
}

uint8_t zf_client_pin_handle_get_pin_token(
    ZerofidoApp *app, Storage *storage, ZfClientPinState *state, const ZfClientPinRequest *request,
    ZfClientPinCommandScratch *scratch, bool permissions_mode, bool require_local_consent,
    ZfTransportSessionId session_id, uint8_t *out, size_t out_capacity, size_t *out_len) {
    uint8_t *protocol_keys = scratch->shared_secret;
    uint8_t *pin_hash_plain = scratch->pin_hash_plain;
    uint8_t *next_pin_token = scratch->next_pin_token;
    uint8_t *encrypted_token = scratch->encrypted_token;
    size_t pin_hash_plain_len = 0;
    size_t encrypted_token_len = 0;
    uint8_t status = ZF_CTAP_SUCCESS;

    status = zf_client_pin_require_existing_pin(state);
    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }
    if (!request->has_key_agreement || !request->has_pin_hash_enc) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    ZF_PIN_OP_DIAG("cmd=%s params", permissions_mode ? "CP-PT" : "CP-TK");
    if (!permissions_mode && (request->has_permissions || request->has_rp_id)) {
        return ZF_CTAP_ERR_INVALID_PARAMETER;
    }
    if (permissions_mode && !request->has_permissions) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    if (permissions_mode && request->permissions == 0U) {
        return ZF_CTAP_ERR_INVALID_PARAMETER;
    }
    if (permissions_mode) {
        const uint64_t supported_permissions = ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA;
        const uint64_t recognized_permissions = supported_permissions | ZF_PIN_PERMISSION_CM |
                                                ZF_PIN_PERMISSION_BE | ZF_PIN_PERMISSION_LBW |
                                                ZF_PIN_PERMISSION_ACFG;

        if ((request->permissions & ~recognized_permissions) != 0U) {
            return ZF_CTAP_ERR_INVALID_PARAMETER;
        }
        if ((request->permissions & ~supported_permissions) != 0U) {
            return ZF_CTAP_ERR_UNAUTHORIZED_PERMISSION;
        }
    }
    if (permissions_mode &&
        (request->permissions & (ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA)) != 0U &&
        !request->has_rp_id) {
        return ZF_CTAP_ERR_MISSING_PARAMETER;
    }
    if (!zf_pin_protocol_supported(request->pin_protocol) ||
        request->pin_hash_enc_len !=
            zf_pin_protocol_encrypted_pin_hash_len(request->pin_protocol)) {
        return ZF_CTAP_ERR_INVALID_PARAMETER;
    }
    ZF_PIN_OP_DIAG("cmd=%s derive", permissions_mode ? "CP-PT" : "CP-TK");
    if (!zf_pin_protocol_derive_keys(state, request->pin_protocol, request->platform_x,
                                     request->platform_y, protocol_keys)) {
        status = ZF_CTAP_ERR_INVALID_PARAMETER;
        goto cleanup;
    }
    zf_client_pin_diag_key_exchange(state, request, protocol_keys);
    zf_client_pin_diag_secret_block("cmd=CP pin hash enc", request->pin_hash_enc,
                                    request->pin_hash_enc_len);
    ZF_PIN_OP_DIAG("cmd=%s decrypt", permissions_mode ? "CP-PT" : "CP-TK");
    if (!zf_pin_protocol_decrypt(request->pin_protocol, protocol_keys, request->pin_hash_enc,
                                 request->pin_hash_enc_len, pin_hash_plain, &pin_hash_plain_len) ||
        pin_hash_plain_len != ZF_PIN_HASH_LEN) {
        status = zf_pin_auth_failure(storage, state);
        goto cleanup;
    }
    ZF_PIN_OP_DIAG("cmd=%s compare", permissions_mode ? "CP-PT" : "CP-TK");
    if (!zf_crypto_constant_time_equal(pin_hash_plain, state->pin_hash, ZF_PIN_HASH_LEN)) {
        zf_client_pin_diag_secret_block("cmd=CP pin hash got", pin_hash_plain, ZF_PIN_HASH_LEN);
        zf_client_pin_diag_secret_block("cmd=CP pin hash stored", state->pin_hash, ZF_PIN_HASH_LEN);
        status = zf_pin_auth_failure(storage, state);
        goto cleanup;
    }

    ZF_PIN_OP_DIAG("cmd=%s success", permissions_mode ? "CP-PT" : "CP-TK");
    if (zf_pin_auth_success(storage, state) != ZF_CTAP_SUCCESS) {
        status = ZF_CTAP_ERR_OTHER;
        goto cleanup;
    }
    if (require_local_consent) {
        ZF_PIN_OP_DIAG("cmd=%s consent", permissions_mode ? "CP-PT" : "CP-TK");
        status = zf_client_pin_request_permission_token_consent(app, state, request, pin_hash_plain,
                                                                session_id);
        if (status != ZF_CTAP_SUCCESS) {
            goto cleanup;
        }
    }
    ZF_PIN_OP_DIAG("cmd=%s token", permissions_mode ? "CP-PT" : "CP-TK");
    zf_pin_refresh_pin_token(next_pin_token);
    if (!zf_pin_protocol_encrypt(request->pin_protocol, protocol_keys, next_pin_token,
                                 ZF_PIN_TOKEN_LEN, encrypted_token,
                                 sizeof(scratch->encrypted_token), &encrypted_token_len)) {
        status = ZF_CTAP_ERR_OTHER;
        goto cleanup;
    }
    status = zf_client_pin_response_token(encrypted_token, encrypted_token_len, out, out_capacity,
                                          out_len);
    if (status == ZF_CTAP_SUCCESS) {
        memcpy(state->pin_token, next_pin_token, sizeof(state->pin_token));
        zf_pin_set_token_permissions(
            state,
            permissions_mode ? request->permissions : (ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA),
            permissions_mode, require_local_consent, permissions_mode ? request->rp_id : NULL);
        zf_pin_note_pin_token_issued(state);
    }

cleanup:
    zf_crypto_secure_zero(protocol_keys, sizeof(scratch->shared_secret));
    zf_crypto_secure_zero(pin_hash_plain, sizeof(scratch->pin_hash_plain));
    zf_crypto_secure_zero(next_pin_token, sizeof(scratch->next_pin_token));
    zf_crypto_secure_zero(encrypted_token, sizeof(scratch->encrypted_token));
    return status;
}
