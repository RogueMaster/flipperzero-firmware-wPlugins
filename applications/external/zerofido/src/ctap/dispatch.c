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

#include "dispatch.h"

#include <string.h>

#include "commands/get_assertion.h"
#include "commands/make_credential.h"
#include "commands/reset.h"
#include "core/approval.h"
#include "core/assertion_queue.h"
#include "core/internal.h"
#include "parse.h"
#include "policy.h"
#include "response.h"
#include "../transport/adapter.h"
#include "../u2f/adapter.h"
#include "../u2f/persistence.h"
#include "../zerofido_app_i.h"
#include "../zerofido_crypto.h"
#include "../zerofido_notify.h"
#include "../zerofido_pin.h"
#include "../zerofido_store.h"
#include "../zerofido_usb_diagnostics.h"

#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS
#define ZF_CTAP_ROUTE_DIAG(text) FURI_LOG_I("ZeroFIDO:CTAP", "route %s", (text))
#else
#define ZF_CTAP_ROUTE_DIAG(text)                                                                   \
    do {                                                                                           \
        (void)(text);                                                                              \
    } while (false)
#endif

static uint8_t zf_handle_selection(ZerofidoApp *app, ZfTransportSessionId session_id,
                                   size_t request_len, size_t *out_len) {
    uint8_t status = zf_ctap_require_empty_payload(request_len);

    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }

    status = zf_ctap_request_approval(app, "Select", "", "Touch required", session_id);
    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }

    *out_len = 0;
    return ZF_CTAP_SUCCESS;
}

/*
 * CTAP command router. GetInfo is read-only; ClientPIN, reset,
 * makeCredential, getAssertion, getNextAssertion, and selection all require
 * the local maintenance gate to be idle before entering their handlers.
 * Empty-payload commands are checked here so handlers can assume their wire
 * shape once called.
 */
uint8_t zf_ctap_dispatch_command(ZerofidoApp *app, const ZfResolvedCapabilities *capabilities,
                                 ZfTransportSessionId session_id, uint8_t cmd,
                                 const uint8_t *request_body, size_t request_body_len,
                                 uint8_t *response_body, size_t response_body_capacity,
                                 size_t *response_body_len) {
    uint8_t status = ZF_CTAP_ERR_INVALID_COMMAND;

    switch (cmd) {
    case ZfCtapeCmdGetInfo: {
        bool pin_is_set = false;

        status = zf_ctap_require_empty_payload(request_body_len + 1U);
        if (status != ZF_CTAP_SUCCESS) {
            break;
        }
        pin_is_set = zf_ctap_pin_is_set(app);
#if ZF_USB_DIAGNOSTICS
        zf_usb_diag_logf(app->storage,
                         "gi caps pin=%u token=%u proto2=%u f21=%u mcuvnr=%u u2f=%u",
                         pin_is_set ? 1U : 0U,
                         capabilities->pin_uv_auth_token_enabled ? 1U : 0U,
                         capabilities->pin_uv_auth_protocol_2_enabled ? 1U : 0U,
                         capabilities->advertise_fido_2_1 ? 1U : 0U,
                         capabilities->make_cred_uv_not_required ? 1U : 0U,
                         capabilities->advertise_u2f_v2 ? 1U : 0U);
#endif
        status = zf_ctap_build_get_info_response(capabilities, pin_is_set, response_body,
                                                 response_body_capacity, response_body_len);
        break;
    }
    case ZfCtapeCmdClientPin:
        status = zf_ctap_dispatch_require_idle(app);
        if (status != ZF_CTAP_SUCCESS) {
            break;
        }
        if (!zf_ctap_begin_maintenance(app)) {
            status = ZF_CTAP_ERR_NOT_ALLOWED;
            break;
        }
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        zf_ctap_assertion_queue_clear(app);
        furi_mutex_release(app->ui_mutex);
        status = zerofido_pin_handle_command_with_session(
            app, session_id, request_body, request_body_len, response_body, response_body_capacity,
            response_body_len);
        zf_ctap_end_maintenance(app);
        break;
    case ZfCtapeCmdReset:
        status = zf_ctap_dispatch_require_idle(app);
        if (status != ZF_CTAP_SUCCESS) {
            break;
        }
        status = zf_ctap_handle_reset(app, session_id, request_body_len + 1U, response_body_len);
        break;
    case ZfCtapeCmdMakeCredential:
        status = zf_ctap_dispatch_require_idle(app);
        if (status != ZF_CTAP_SUCCESS) {
            break;
        }
        status = zf_ctap_handle_make_credential(app, session_id, request_body, request_body_len,
                                                response_body, response_body_capacity,
                                                response_body_len);
        break;
    case ZfCtapeCmdGetAssertion:
        ZF_CTAP_ROUTE_DIAG("GA gate");
        status = zf_ctap_dispatch_require_idle(app);
        if (status != ZF_CTAP_SUCCESS) {
            break;
        }
        ZF_CTAP_ROUTE_DIAG("GA call");
        status =
            zf_ctap_handle_get_assertion(app, session_id, request_body, request_body_len,
                                         response_body, response_body_capacity, response_body_len);
        break;
    case ZfCtapeCmdGetNextAssertion:
        status = zf_ctap_dispatch_require_idle(app);
        if (status != ZF_CTAP_SUCCESS) {
            break;
        }
        status = zf_ctap_require_empty_payload(request_body_len + 1U);
        if (status != ZF_CTAP_SUCCESS) {
            break;
        }
        status = zf_ctap_assertion_queue_handle_next(app, session_id, response_body,
                                                     response_body_capacity, response_body_len);
        break;
    case ZfCtapeCmdSelection:
        status = zf_ctap_dispatch_require_idle(app);
        if (status != ZF_CTAP_SUCCESS) {
            break;
        }
        status = zf_handle_selection(app, session_id, request_body_len + 1U, response_body_len);
        break;
    default:
        *response_body_len = 0;
        break;
    }

    return status;
}
