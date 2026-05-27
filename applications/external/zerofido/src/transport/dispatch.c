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

#ifndef ZF_NFC_ONLY

#include <string.h>

#include "../zerofido_app_i.h"
#include "../zerofido_ctap.h"
#include "../zerofido_notify.h"
#include "../zerofido_runtime_config.h"
#include "../zerofido_telemetry.h"
#include "../zerofido_ui.h"
#include "../u2f/adapter.h"

static bool zf_transport_dispatch_was_interrupted(const ZfTransportState *transport,
                                                  uint32_t generation) {
    return generation != transport->processing_generation;
}

static void zf_transport_dispatch_error(ZfProtocolDispatchResult *result, uint8_t hid_error) {
    uint8_t *response = result->response;
    size_t response_capacity = result->response_capacity;

    memset(result, 0, sizeof(*result));
    result->response = response;
    result->response_capacity = response_capacity;
    result->send_transport_error = true;
    result->transport_error = hid_error;
}

static void zf_transport_dispatch_begin(ZfProtocolDispatchResult *result, uint8_t *response,
                                        size_t response_capacity) {
    memset(result, 0, sizeof(*result));
    result->response = response;
    result->response_capacity = response_capacity;
}

static void zf_transport_dispatch_reject_unsupported(ZerofidoApp *app,
                                                     ZfProtocolDispatchResult *result) {
    zerofido_notify_error(app);
    zf_transport_dispatch_error(result, ZF_HID_ERR_INVALID_CMD);
}

static void zf_transport_dispatch_ping(const ZfProtocolDispatchRequest *request,
                                       ZfProtocolDispatchResult *result) {
    zf_transport_dispatch_begin(result, result->response, result->response_capacity);
    result->response_len = request->payload_len;
    if (request->payload_len > 0 && result->response != request->payload) {
        memcpy(result->response, request->payload, request->payload_len);
    }
}

static void zf_transport_dispatch_u2f(ZerofidoApp *app, const ZfProtocolDispatchRequest *request,
                                      ZfProtocolDispatchResult *result) {
    zf_transport_dispatch_begin(result, result->response, result->response_capacity);
    result->response_len =
        zf_u2f_adapter_handle_msg(app, request->session_id, request->payload, request->payload_len,
                                  result->response, result->response_capacity);
}

static void zf_transport_dispatch_cbor(ZerofidoApp *app, const ZfProtocolDispatchRequest *request,
                                       ZfProtocolDispatchResult *result) {
    zf_transport_dispatch_begin(result, result->response, result->response_capacity);
    zf_telemetry_log("ctap transport before");
    result->response_len =
        zerofido_handle_ctap2(app, request->session_id, request->payload, request->payload_len,
                              result->response, result->response_capacity);
    zf_telemetry_log("ctap transport after");
}

static void zf_transport_dispatch_wink(ZerofidoApp *app, ZfProtocolDispatchResult *result) {
    zf_transport_dispatch_begin(result, result->response, result->response_capacity);
    zf_u2f_adapter_wink(app);
}

static void zf_transport_dispatch_abort_if_interrupted(ZfTransportState *transport,
                                                       uint32_t generation) {
    if (zf_transport_dispatch_was_interrupted(transport, generation)) {
        return;
    }

    transport->processing = false;
}

/*
 * Routes a fully assembled transport message into the selected protocol. The
 * processing generation/session guards let workers discard late responses after
 * CANCEL, channel resync, disconnect, or a new NFC selection.
 */
void zf_transport_dispatch_complete_message(ZerofidoApp *app, ZfTransportState *transport,
                                            ZfTransportSessionId session_id,
                                            ZfTransportProtocolKind protocol,
                                            const uint8_t *payload, size_t payload_len) {
    ZfResolvedCapabilities capabilities;
    ZfProtocolDispatchRequest request = {
        .session_id = session_id,
        .protocol = protocol,
        .payload = payload,
        .payload_len = payload_len,
    };
    ZfProtocolDispatchResult result = {0};
    uint32_t generation = transport->processing_generation + 1;

    if (!zf_app_transport_arena_acquire(app)) {
        zf_telemetry_log_oom("dispatch transport arena", ZF_TRANSPORT_ARENA_SIZE);
        zf_transport_session_send_error(session_id, ZF_HID_ERR_OTHER);
        return;
    }

    result.response = app->transport_arena;
    result.response_capacity = ZF_MAX_MSG_SIZE;

    transport->processing_generation = generation;
    transport->processing = true;
    transport->processing_resync = false;
    zf_runtime_get_effective_capabilities(app, &capabilities);

    switch (request.protocol) {
    case ZfTransportProtocolKindPing:
        zf_transport_dispatch_ping(&request, &result);
        break;
    case ZfTransportProtocolKindU2f:
        if (!capabilities.u2f_enabled) {
            zf_transport_dispatch_reject_unsupported(app, &result);
            break;
        }
        zf_transport_dispatch_u2f(app, &request, &result);
        break;
    case ZfTransportProtocolKindWink:
        if (!capabilities.transport_wink_enabled) {
            zf_transport_dispatch_reject_unsupported(app, &result);
            break;
        }
        zf_transport_dispatch_wink(app, &result);
        break;
    case ZfTransportProtocolKindCtap2:
        if (!capabilities.fido2_enabled) {
            zf_transport_dispatch_reject_unsupported(app, &result);
            break;
        }
        zf_transport_dispatch_cbor(app, &request, &result);
        break;
    default:
        zf_transport_dispatch_reject_unsupported(app, &result);
        break;
    }

    if (zf_transport_dispatch_was_interrupted(transport, generation)) {
        return;
    }

    if (request.protocol == ZfTransportProtocolKindCtap2 &&
        transport->processing_cancel_requested) {
        result.send_transport_error = false;
        if (result.response && result.response_capacity > 0U) {
            result.response[0] = ZF_CTAP_ERR_KEEPALIVE_CANCEL;
            result.response_len = 1;
        } else {
            zf_transport_dispatch_error(&result, ZF_HID_ERR_OTHER);
        }
    }

    zf_transport_send_dispatch_result(app, &request, &result);
    if (request.protocol == ZfTransportProtocolKindCtap2) {
        zerofido_ui_refresh_status(app);
    }

    zf_transport_dispatch_abort_if_interrupted(transport, generation);
}

#endif
