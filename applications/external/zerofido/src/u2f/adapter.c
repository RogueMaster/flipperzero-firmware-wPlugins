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

#include "adapter.h"

#include <stdio.h>
#include <string.h>

#include "apdu.h"
#include "apdu_internal.h"
#include "session.h"
#include "persistence.h"
#include "status.h"
#include "../zerofido_app_i.h"
#include "../zerofido_crypto.h"
#include "../zerofido_notify.h"
#include "../zerofido_runtime_config.h"
#include "../zerofido_ui.h"

#define ZF_U2F_VERSION_RESPONSE_LEN 8U

static const uint8_t zf_u2f_adapter_version_response[] = {'U', '2', 'F', '_', 'V', '2', 0x90, 0x00};

static uint16_t zf_u2f_adapter_reply_version(uint8_t *response, size_t response_capacity) {
    if (response_capacity < ZF_U2F_VERSION_RESPONSE_LEN) {
        return zf_u2f_write_status(response, ZF_U2F_SW_WRONG_LENGTH);
    }

    memcpy(response, zf_u2f_adapter_version_response, sizeof(zf_u2f_adapter_version_response));
    return sizeof(zf_u2f_adapter_version_response);
}

bool zf_u2f_adapter_ensure_attestation_assets(void) {
    uint8_t loaded_cert_key[ZF_PRIVATE_KEY_LEN];
    bool assets_ready = false;

    zf_crypto_secure_zero(loaded_cert_key, sizeof(loaded_cert_key));
    if (u2f_data_check(true) && u2f_data_cert_check() && u2f_data_cert_key_load(loaded_cert_key) &&
        u2f_data_cert_key_matches(loaded_cert_key)) {
        assets_ready = true;
    }
    zf_crypto_secure_zero(loaded_cert_key, sizeof(loaded_cert_key));

    if (assets_ready) {
        return true;
    }

    return u2f_data_generate_attestation_assets();
}

static void zf_u2f_format_app_id(const uint8_t *request, size_t request_len, char *out,
                                 size_t out_len) {
    U2fParsedApdu apdu = {0};
    const uint8_t *app_id = NULL;

    if (out_len < 3 || request_len > UINT16_MAX ||
        !u2f_parse_apdu_header(request, (uint16_t)request_len, false, &apdu) || !apdu.data ||
        apdu.lc < (U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE)) {
        strncpy(out, "U2F", out_len - 1);
        out[out_len - 1] = '\0';
        return;
    }
    app_id = apdu.data + U2F_CHALLENGE_SIZE;

    snprintf(out, out_len, "app %02x%02x%02x%02x%02x%02x%02x%02x...", app_id[0], app_id[1],
             app_id[2], app_id[3], app_id[4], app_id[5], app_id[6], app_id[7]);
}

/*
 * Prompts only for U2F operations that require user presence: REGISTER and
 * AUTHENTICATE with enforce-user-presence. Check-only and dont-enforce requests
 * remain protocol probes and do not consume a touch here.
 */
static bool zf_u2f_request_approval(ZerofidoApp *app, ZfTransportSessionId session_id,
                                    const uint8_t *request, uint16_t request_len) {
    const char *operation = NULL;
    if (!zf_u2f_adapter_is_available(app)) {
        return false;
    }
    if (!u2f_request_needs_user_presence(request, request_len, &operation)) {
        return true;
    }

    char rp_text[48];
    bool approved = false;
    zf_u2f_format_app_id(request, request_len, rp_text, sizeof(rp_text));
    if (!zerofido_ui_request_approval(app, ZfUiProtocolU2f, operation, rp_text, "Touch required",
                                      session_id, &approved)) {
        return false;
    }

    if (!approved) {
        return false;
    }

    u2f_confirm_user_present(app->u2f);
    return true;
}

static void zf_u2f_event_callback(U2fNotifyEvent evt, void *context) {
    ZerofidoApp *app = context;

    furi_assert(app);

    switch (evt) {
    case U2fNotifyAuthSuccess:
        zerofido_notify_success(app);
        break;
    case U2fNotifyWink:
        zerofido_notify_wink(app);
        break;
    case U2fNotifyError:
        zerofido_notify_error(app);
        break;
    case U2fNotifyRegister:
    case U2fNotifyAuth:
    case U2fNotifyConnect:
    case U2fNotifyDisconnect:
    default:
        break;
    }
}

bool zf_u2f_adapter_init(ZerofidoApp *app) {
    ZfResolvedCapabilities capabilities;

    if (!app) {
        return false;
    }
    if (app->u2f) {
        return true;
    }
    zf_runtime_get_effective_capabilities(app, &capabilities);
    if (!capabilities.u2f_enabled) {
        return true;
    }

    app->u2f = u2f_alloc();
    if (!app->u2f) {
        return false;
    }

    (void)zf_u2f_adapter_ensure_attestation_assets();

    if (!u2f_init(app->u2f)) {
        u2f_free(app->u2f);
        app->u2f = NULL;
        return false;
    }

    u2f_set_event_callback(app->u2f, zf_u2f_event_callback, app);
    return true;
}

bool zf_u2f_adapter_ensure_init(ZerofidoApp *app) {
    if (!app) {
        return false;
    }

    if (app->u2f) {
        return true;
    }

    return zf_u2f_adapter_init(app);
}

void zf_u2f_adapter_deinit(ZerofidoApp *app) {
    if (!app->u2f) {
        return;
    }

    u2f_free(app->u2f);
    app->u2f = NULL;
}

bool zf_u2f_adapter_is_available(const ZerofidoApp *app) {
    return app && app->u2f;
}

void zf_u2f_adapter_set_connected(ZerofidoApp *app, bool connected) {
    if (!app->u2f) {
        return;
    }

    u2f_set_state(app->u2f, connected ? 1 : 0);
}

/*
 * Bridges transport APDUs to the U2F session. The request is copied into the
 * response buffer for in-place parsing/signing; VERSION can answer before full
 * U2F init so hosts can still detect the legacy surface.
 */
size_t zf_u2f_adapter_handle_msg(ZerofidoApp *app, ZfTransportSessionId session_id,
                                 const uint8_t *request, size_t request_len, uint8_t *response,
                                 size_t response_capacity) {
    ZfResolvedCapabilities capabilities;
    uint16_t validation_status = 0;

    zf_runtime_get_effective_capabilities(app, &capabilities);
    if (!capabilities.u2f_enabled || request_len == 0 || response_capacity < 2) {
        return 0;
    }
    if (request_len > UINT16_MAX) {
        return zf_u2f_write_status(response, ZF_U2F_SW_WRONG_LENGTH);
    }

    validation_status = u2f_validate_request_into_response(request, (uint16_t)request_len, response,
                                                           (uint16_t)response_capacity);
    if (validation_status != 0) {
        return validation_status;
    }
    if (request_len > response_capacity) {
        return zf_u2f_write_status(response, ZF_U2F_SW_WRONG_LENGTH);
    }
    if (request[1] == U2F_CMD_VERSION && !zf_u2f_adapter_is_available(app)) {
        return zf_u2f_adapter_reply_version(response, response_capacity);
    }

    if (response != request) {
        memcpy(response, request, request_len);
    }
    if (!zf_u2f_adapter_ensure_init(app)) {
        return zf_u2f_write_status(response, ZF_U2F_SW_INS_NOT_SUPPORTED);
    }
    if (response_capacity < ZF_U2F_VERSION_RESPONSE_LEN) {
        return 0;
    }
    if (!zf_u2f_request_approval(app, session_id, response, (uint16_t)request_len)) {
        return zf_u2f_write_status(response, ZF_U2F_SW_CONDITIONS_NOT_SATISFIED);
    }

    validation_status =
        u2f_msg_parse(app->u2f, response, (uint16_t)request_len, (uint16_t)response_capacity);
    u2f_clear_user_present(app->u2f);
    return validation_status;
}

void zf_u2f_adapter_wink(ZerofidoApp *app) {
    if (zf_u2f_adapter_ensure_init(app) && app->u2f) {
        u2f_wink(app->u2f);
    }
}
