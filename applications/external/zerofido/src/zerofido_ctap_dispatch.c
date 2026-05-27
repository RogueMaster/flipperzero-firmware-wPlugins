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

#include "zerofido_ctap.h"

#include "ctap/dispatch.h"
#include "ctap/policy.h"
#include "zerofido_app_i.h"
#include "zerofido_runtime_config.h"
#include "zerofido_telemetry.h"
#include "zerofido_usb_diagnostics.h"

#if ZF_RELEASE_DIAGNOSTICS || ZF_USB_DIAGNOSTICS
#define ZF_CTAP_LOG_TAG "ZeroFIDO:CTAP"

static const char *zf_ctap_command_name(uint8_t cmd) {
    switch (cmd) {
    case ZfCtapeCmdGetInfo:
        return "GI";
    case ZfCtapeCmdClientPin:
        return "CP";
    case ZfCtapeCmdReset:
        return "RST";
    case ZfCtapeCmdMakeCredential:
        return "MC";
    case ZfCtapeCmdGetAssertion:
        return "GA";
    case ZfCtapeCmdGetNextAssertion:
        return "GN";
    case ZfCtapeCmdSelection:
        return "SEL";
    default:
        return "UK";
    }
}

static const char *zf_ctap_status_name(uint8_t status) {
    switch (status) {
    case ZF_CTAP_SUCCESS:
        return "OK";
    case ZF_CTAP_ERR_INVALID_COMMAND:
        return "ICMD";
    case ZF_CTAP_ERR_INVALID_PARAMETER:
        return "IPRM";
    case ZF_CTAP_ERR_INVALID_LENGTH:
        return "LEN";
    case ZF_CTAP_ERR_INVALID_CHANNEL:
        return "CHAN";
    case ZF_CTAP_ERR_CBOR_UNEXPECTED_TYPE:
    case ZF_CTAP_ERR_INVALID_CBOR:
        return "CBOR";
    case ZF_CTAP_ERR_MISSING_PARAMETER:
        return "MISS";
    case ZF_CTAP_ERR_CREDENTIAL_EXCLUDED:
        return "EXCL";
    case ZF_CTAP_ERR_UNSUPPORTED_ALGORITHM:
        return "ALG";
    case ZF_CTAP_ERR_OPERATION_DENIED:
        return "DENY";
    case ZF_CTAP_ERR_KEY_STORE_FULL:
        return "FULL";
    case ZF_CTAP_ERR_UNSUPPORTED_OPTION:
        return "UOPT";
    case ZF_CTAP_ERR_INVALID_OPTION:
        return "IOPT";
    case ZF_CTAP_ERR_KEEPALIVE_CANCEL:
        return "CANCEL";
    case ZF_CTAP_ERR_NO_CREDENTIALS:
        return "NOCRED";
    case ZF_CTAP_ERR_USER_ACTION_TIMEOUT:
        return "TIME";
    case ZF_CTAP_ERR_NOT_ALLOWED:
        return "NALLOW";
    case ZF_CTAP_ERR_PIN_INVALID:
        return "PIN";
    case ZF_CTAP_ERR_PIN_BLOCKED:
        return "PBLK";
    case ZF_CTAP_ERR_PIN_AUTH_INVALID:
        return "PAUTH";
    case ZF_CTAP_ERR_PIN_AUTH_BLOCKED:
        return "PABLK";
    case ZF_CTAP_ERR_PIN_NOT_SET:
        return "PNONE";
    case ZF_CTAP_ERR_PIN_REQUIRED:
        return "PREQ";
    case ZF_CTAP_ERR_PIN_POLICY_VIOLATION:
        return "PPOL";
    case ZF_CTAP_ERR_PIN_TOKEN_EXPIRED:
        return "PTOK";
    case ZF_CTAP_ERR_INVALID_SUBCOMMAND:
        return "SUB";
    case ZF_CTAP_ERR_UNAUTHORIZED_PERMISSION:
        return "UPERM";
    case ZF_CTAP_ERR_OTHER:
        return "OTHER";
    default:
        return "UNK";
    }
}
#endif

#if ZF_RELEASE_DIAGNOSTICS
#define ZF_CTAP_DIAG(...) FURI_LOG_I(ZF_CTAP_LOG_TAG, __VA_ARGS__)

static void zf_ctap_note_result(ZerofidoApp *app, uint8_t cmd, uint8_t status, size_t body_len) {
    if (!app) {
        return;
    }
    if (app->transport_auto_accept_transaction) {
        return;
    }
    if (cmd == ZfCtapeCmdClientPin) {
        return;
    }

    FURI_LOG_I(ZF_CTAP_LOG_TAG, "cmd=%s status=%s body=%u", zf_ctap_command_name(cmd),
               zf_ctap_status_name(status), (unsigned)body_len);
}
#else
#define ZF_CTAP_DIAG(...)                                                                          \
    do {                                                                                           \
    } while (false)

static void zf_ctap_note_result(ZerofidoApp *app, uint8_t cmd, uint8_t status, size_t body_len) {
    (void)app;
    (void)cmd;
    (void)status;
    (void)body_len;
}
#endif

#if ZF_USB_DIAGNOSTICS
static void zf_ctap_usb_note_result(ZerofidoApp *app, uint8_t cmd, uint8_t status,
                                    size_t body_len) {
    if (!app || app->transport_auto_accept_transaction) {
        return;
    }
    zf_usb_diag_logf(app->storage, "ctap cmd=%s status=%s body=%u", zf_ctap_command_name(cmd),
                     zf_ctap_status_name(status), (unsigned)body_len);
}
#endif

/*
 * CTAP2 transport entry point. The incoming buffer is the CTAP command byte
 * followed by the command-specific CBOR body; the outgoing buffer always starts
 * with one CTAP status byte followed by the response body on success.
 *
 * Runtime capability/profile checks happen before command dispatch so disabled
 * commands fail without entering stateful handlers. On error, any response body
 * already written by a lower layer is intentionally discarded.
 */
size_t zerofido_handle_ctap2(ZerofidoApp *app, ZfTransportSessionId session_id,
                             const uint8_t *request, size_t request_len, uint8_t *response,
                             size_t response_capacity) {
    uint8_t status = ZF_CTAP_ERR_INVALID_COMMAND;
    size_t body_len = 0;
    size_t body_capacity = response_capacity - 1;
    ZfResolvedCapabilities capabilities;
    uint8_t cmd = 0U;

    if (request_len == 0 || response_capacity <= 1) {
        return 0;
    }

    cmd = request[0];
    ZF_CTAP_DIAG("start cmd=%s len=%u", zf_ctap_command_name(cmd), (unsigned)request_len);
#if ZF_USB_DIAGNOSTICS
    if (app && !app->transport_auto_accept_transaction) {
        zf_usb_diag_logf(app->storage, "ctap start cmd=%s len=%u", zf_ctap_command_name(cmd),
                         (unsigned)request_len);
    }
#endif
    zf_runtime_get_effective_capabilities(app, &capabilities);
    if (!zf_runtime_ctap_command_enabled(app, cmd)) {
        response[0] = ZF_CTAP_ERR_INVALID_COMMAND;
        zf_ctap_note_result(app, cmd, ZF_CTAP_ERR_INVALID_COMMAND, 0U);
#if ZF_USB_DIAGNOSTICS
        zf_ctap_usb_note_result(app, cmd, ZF_CTAP_ERR_INVALID_COMMAND, 0U);
#endif
        return 1;
    }

    ZF_CTAP_DIAG("dispatch cmd=%s", zf_ctap_command_name(cmd));
    zf_telemetry_log("ctap dispatch before");
    status = zf_ctap_dispatch_command(app, &capabilities, session_id, cmd, request + 1,
                                      request_len - 1, response + 1, body_capacity, &body_len);
    zf_telemetry_log("ctap dispatch after");

    if (status != ZF_CTAP_SUCCESS) {
        body_len = 0;
    }

    response[0] = status;
    zf_ctap_note_result(app, cmd, status, body_len);
#if ZF_USB_DIAGNOSTICS
    zf_ctap_usb_note_result(app, cmd, status, body_len);
#endif
    ZF_CTAP_DIAG("done cmd=%s status=%s body=%u", zf_ctap_command_name(cmd),
                 zf_ctap_status_name(status), (unsigned)body_len);
    return body_len + 1;
}
