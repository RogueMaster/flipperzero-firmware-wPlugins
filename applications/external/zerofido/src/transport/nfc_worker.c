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

#ifndef ZF_USB_ONLY

#include "nfc_worker.h"

#include <furi.h>
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS && !defined(ZF_HOST_TEST)
#include <furi/core/message_queue.h>
#endif
#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <string.h>
#include <toolbox/bit_buffer.h>

#include "../u2f/adapter.h"
#include "../zerofido_app_i.h"
#include "../zerofido_ctap.h"
#include "../zerofido_crypto.h"
#include "../zerofido_telemetry.h"
#include "../zerofido_ui.h"
#include "nfc_engine.h"
#include "nfc_iso_dep.h"
#include "nfc_session.h"
#include "nfc_trace.h"
#include "usb_hid_session.h"

#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS && !defined(ZF_HOST_TEST)
#define ZF_NFC_MEM_DIAG(event) zf_telemetry_log(event)
#else
#define ZF_NFC_MEM_DIAG(event)                                                                     \
    do {                                                                                           \
    } while (false)
#endif

static ZfNfcTransportState *zf_transport_nfc_state(ZerofidoApp *app) {
    return app ? (ZfNfcTransportState *)app->transport_state : NULL;
}

static ZfNfcTransportState *zf_transport_nfc_worker_state_alloc(ZerofidoApp *app) {
#ifdef ZF_HOST_TEST
    ZfNfcTransportState *state = zf_app_nfc_transport_state(app);
#else
    ZfNfcTransportState *state = malloc(sizeof(*state));
#endif

    if (!state) {
        zf_telemetry_log_oom("nfc state", sizeof(*state));
        return NULL;
    }

    memset(state, 0, sizeof(*state));
#ifndef ZF_HOST_TEST
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    zf_app_set_nfc_transport_state(app, state);
    furi_mutex_release(app->ui_mutex);
#endif
    return state;
}

static void zf_transport_nfc_worker_state_free(ZerofidoApp *app, ZfNfcTransportState *state) {
    if (!state) {
        return;
    }

    zf_crypto_secure_zero(state, sizeof(*state));
#ifndef ZF_HOST_TEST
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (zf_app_nfc_transport_state(app) == state) {
        zf_app_set_nfc_transport_state(app, NULL);
    }
    furi_mutex_release(app->ui_mutex);
    free(state);
#else
    (void)app;
#endif
}

static void zf_transport_nfc_signal_worker(ZerofidoApp *app, uint32_t flags,
                                           bool caller_holds_ui_mutex) {
    FuriThread *worker_thread = NULL;
    FuriThreadId id = 0;

    if (!app) {
        return;
    }

    if (!caller_holds_ui_mutex) {
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    }
    worker_thread = app->worker_thread;
    id = worker_thread ? furi_thread_get_id(worker_thread) : 0;
    if (!caller_holds_ui_mutex) {
        furi_mutex_release(app->ui_mutex);
    }
    if (id) {
        furi_thread_flags_set(id, flags);
    }
}

static uint8_t *zf_transport_nfc_response_buffer_alloc(size_t *out_capacity) {
    uint8_t *buffer = malloc(ZF_TRANSPORT_ARENA_SIZE);

    if (out_capacity) {
        *out_capacity = 0U;
    }
    if (!buffer) {
        zf_telemetry_log_oom("nfc response buffer", ZF_TRANSPORT_ARENA_SIZE);
        return NULL;
    }

    memset(buffer, 0, ZF_TRANSPORT_ARENA_SIZE);
    if (out_capacity) {
        *out_capacity = ZF_TRANSPORT_ARENA_SIZE;
    }
    return buffer;
}

static void zf_transport_nfc_response_buffer_free(uint8_t *buffer, size_t capacity) {
    if (!buffer) {
        return;
    }

    zf_crypto_secure_zero(buffer, capacity);
    free(buffer);
}

/*
 * Worker-side processing for queued NFC CTAP2/U2F requests. Responses are stored
 * only if the generation/session still match, preventing late completions from
 * answering a newer applet selection or exchange.
 */
static void zf_transport_nfc_process_request(ZerofidoApp *app) {
    ZfNfcTransportState *state = zf_app_nfc_transport_state(app);
    size_t request_len = 0;
    size_t response_len = 0;
    ZfTransportSessionId session_id = 0;
    ZfNfcRequestKind request_kind = ZfNfcRequestKindNone;
    uint32_t request_generation = 0U;
    uint8_t *request = NULL;
    uint8_t *response = NULL;
    uint8_t request_copy[ZF_MAX_MSG_SIZE];
    size_t response_capacity = 0U;
    bool old_auto_accept = false;

    memset(request_copy, 0, sizeof(request_copy));
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    request = zf_transport_nfc_arena(state);
    if (!state->request_pending || !request || state->request_len > ZF_MAX_MSG_SIZE) {
        furi_mutex_release(app->ui_mutex);
        return;
    }

    request_len = state->request_len;
    session_id = state->processing_session_id;
    request_kind = state->request_kind;
    request_generation = state->processing_generation;
    memcpy(request_copy, request, request_len);
    state->request_pending = false;
    old_auto_accept = app->transport_auto_accept_transaction;
    app->transport_auto_accept_transaction = true;
    zf_transport_nfc_attach_arena(state, NULL, 0U);
    zf_app_transport_arena_release(app);
    furi_mutex_release(app->ui_mutex);

    response = zf_transport_nfc_response_buffer_alloc(&response_capacity);
    if (!response || response_capacity <= 1U) {
        zf_transport_nfc_store_response(app, state, session_id, request_generation, NULL, 0,
                                        request_kind == ZfNfcRequestKindU2f, true,
                                        ZF_NFC_SW_INTERNAL_ERROR);
        goto cleanup;
    }

    switch (request_kind) {
    case ZfNfcRequestKindCtap2:
        zf_transport_nfc_trace_event("CTAP2 worker start");
        zf_transport_nfc_trace_event("CTAP2 worker dispatch");
        response_len = zerofido_handle_ctap2(
            app, session_id, request_copy, request_len, response,
            response_capacity > ZF_MAX_MSG_SIZE ? ZF_MAX_MSG_SIZE : response_capacity);
        zf_transport_nfc_trace_event("CTAP2 worker handled");
        zerofido_ui_refresh_status(app);
        zf_transport_nfc_trace_event(response_len == 0U ? "CTAP2 worker empty"
                                                        : "CTAP2 worker done");
        zf_transport_nfc_store_response(
            app, state, session_id, request_generation, response, response_len, false,
            response_len == 0, response_len == 0 ? ZF_NFC_SW_INTERNAL_ERROR : ZF_NFC_SW_SUCCESS);
        break;
    case ZfNfcRequestKindU2f:
        response_len = zf_u2f_adapter_handle_msg(app, session_id, request_copy, request_len,
                                                 response, response_capacity);
        zf_transport_nfc_store_response(
            app, state, session_id, request_generation, response, response_len, true,
            response_len == 0, response_len == 0 ? ZF_NFC_SW_INTERNAL_ERROR : ZF_NFC_SW_SUCCESS);
        break;
    case ZfNfcRequestKindNone:
    default:
        zf_transport_nfc_store_response(app, state, session_id, request_generation, NULL, 0, false,
                                        true, ZF_NFC_SW_INTERNAL_ERROR);
        break;
    }
cleanup:
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (zf_app_nfc_transport_state(app) == state) {
        app->transport_auto_accept_transaction = old_auto_accept;
    }
    furi_mutex_release(app->ui_mutex);
    zf_transport_nfc_response_buffer_free(response, response_capacity);
    zf_crypto_secure_zero(request_copy, sizeof(request_copy));
    (void)request_len;
}

bool zf_transport_nfc_wake_request_worker(ZerofidoApp *app, ZfNfcTransportState *state,
                                          bool caller_holds_ui_mutex) {
    if (!app || !state) {
        return false;
    }

    if (caller_holds_ui_mutex && zf_app_nfc_transport_state(app) != state) {
        return false;
    }
    if (state->stopping || !state->request_pending) {
        return false;
    }

    zf_transport_nfc_signal_worker(app, ZF_NFC_WORKER_EVT_REQUEST, caller_holds_ui_mutex);
    return true;
}

int32_t zf_transport_nfc_worker(void *context) {
    ZerofidoApp *app = context;
    ZfNfcTransportState *state = NULL;

    if (!app) {
        return 0;
    }

    zf_telemetry_log("nfc worker start");
    state = zf_transport_nfc_worker_state_alloc(app);
    if (!state) {
        zerofido_ui_set_status(app, "NFC init failed");
        return 0;
    }

    if (!zf_app_transport_arena_acquire(app)) {
        zf_telemetry_log_oom("nfc transport arena", ZF_TRANSPORT_ARENA_SIZE);
        zf_transport_nfc_worker_state_free(app, state);
        zerofido_ui_set_status(app, "NFC init failed");
        return 0;
    }
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS && !defined(ZF_HOST_TEST)
    state->trace_queue =
        furi_message_queue_alloc(ZF_NFC_TRACE_QUEUE_DEPTH, sizeof(ZfNfcTraceRecord));
#endif
    zf_transport_nfc_attach_arena(state, app->transport_arena,
                                  zf_app_transport_arena_capacity(app));
    ZF_NFC_MEM_DIAG("nfc alloc nfc before");
    state->nfc = nfc_alloc();
    ZF_NFC_MEM_DIAG("nfc alloc nfc after");
    ZF_NFC_MEM_DIAG("nfc alloc bitbuf before");
    state->tx_buffer = bit_buffer_alloc(ZF_NFC_TX_FRAME_CAPACITY);
    ZF_NFC_MEM_DIAG("nfc alloc bitbuf after");
    ZF_NFC_MEM_DIAG("nfc alloc iso4a before");
    state->iso14443_4a_data = iso14443_4a_alloc();
    ZF_NFC_MEM_DIAG("nfc alloc iso4a after");
    if (!state->nfc || !state->tx_buffer || !state->iso14443_4a_data) {
        zf_telemetry_log_oom("nfc resources", 0U);
        zerofido_ui_set_status(app, "NFC init failed");
        if (state->iso14443_4a_data) {
            iso14443_4a_free(state->iso14443_4a_data);
        }
        if (state->tx_buffer) {
            bit_buffer_free(state->tx_buffer);
        }
        if (state->nfc) {
            nfc_free(state->nfc);
        }
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS && !defined(ZF_HOST_TEST)
        if (state->trace_queue) {
            furi_message_queue_free(state->trace_queue);
        }
#endif
        zf_app_transport_arena_release(app);
        zf_transport_nfc_worker_state_free(app, state);
        return 0;
    }

    zf_transport_nfc_prepare_listener(state);
    ZF_NFC_MEM_DIAG("nfc alloc listener before");
    state->listener = nfc_listener_alloc(state->nfc, NfcProtocolIso14443_3a,
                                         iso14443_4a_get_base_data(state->iso14443_4a_data));
    ZF_NFC_MEM_DIAG("nfc alloc listener after");
    if (!state->listener) {
        zf_telemetry_log_oom("nfc listener", 0U);
        zerofido_ui_set_status(app, "NFC listener failed");
        iso14443_4a_free(state->iso14443_4a_data);
        bit_buffer_free(state->tx_buffer);
        nfc_free(state->nfc);
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS && !defined(ZF_HOST_TEST)
        if (state->trace_queue) {
            furi_message_queue_free(state->trace_queue);
        }
#endif
        zf_app_transport_arena_release(app);
        zf_transport_nfc_worker_state_free(app, state);
        return 0;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->transport_state = state;
    state->listener_active = true;
    state->field_active = false;
    state->iso4_active = false;
    state->applet_selected = false;
    state->iso_pcb = ZF_NFC_PCB_BLOCK;
    state->last_visible_stage = ZfNfcUiStageWaiting;
    state->last_visible_stage_tick = furi_get_tick();
    furi_mutex_release(app->ui_mutex);
    zf_transport_nfc_trace_bind(state->trace_queue, furi_thread_get_current_id());
    nfc_listener_start(state->listener, zf_transport_nfc_event_callback, app);
    zerofido_ui_refresh_status_line(app);

    while (true) {
        uint32_t flags = furi_thread_flags_wait(ZF_NFC_WORKER_EVT_STOP | ZF_NFC_WORKER_EVT_REQUEST |
                                                    ZF_NFC_WORKER_EVT_TRACE,
                                                FuriFlagWaitAny, FuriWaitForever);
        if ((flags & FuriFlagError) != 0) {
            if (flags == FuriFlagErrorTimeout) {
                continue;
            }
            break;
        }
        if ((flags & ZF_NFC_WORKER_EVT_TRACE) != 0U) {
            zf_transport_nfc_trace_drain(state->trace_queue);
        }
        if ((flags & ZF_NFC_WORKER_EVT_STOP) != 0U) {
            break;
        }
        if ((flags & ZF_NFC_WORKER_EVT_REQUEST) != 0U) {
            zf_transport_nfc_process_request(app);
            zf_transport_nfc_trace_drain(state->trace_queue);
        }
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    state->stopping = true;
    zf_transport_nfc_cancel_current_request_locked(state);
    state->listener_active = false;
    furi_mutex_release(app->ui_mutex);
    furi_semaphore_release(app->approval.done);
    nfc_listener_stop(state->listener);
    zf_transport_nfc_trace_drain(state->trace_queue);
    zf_transport_nfc_trace_unbind(state->trace_queue);
    zf_transport_nfc_on_disconnect(app);
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (app->transport_state == state) {
        app->transport_state = NULL;
    }
    furi_mutex_release(app->ui_mutex);
    ZF_NFC_MEM_DIAG("nfc worker stop release before");
    nfc_listener_free(state->listener);
    iso14443_4a_free(state->iso14443_4a_data);
    bit_buffer_free(state->tx_buffer);
    nfc_free(state->nfc);
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS && !defined(ZF_HOST_TEST)
    if (state->trace_queue) {
        furi_message_queue_free(state->trace_queue);
    }
#endif
    zf_app_transport_arena_release(app);
    zf_transport_nfc_worker_state_free(app, state);
    ZF_NFC_MEM_DIAG("nfc worker stop release after");
    zf_telemetry_log("nfc worker stop");
    return 0;
}

void zf_transport_nfc_stop(ZerofidoApp *app) {
    if (app) {
        ZfNfcTransportState *state = NULL;

        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        state = zf_app_nfc_transport_state(app);
        if (state) {
            state->stopping = true;
            zf_transport_nfc_cancel_current_request_locked(state);
        }
        furi_mutex_release(app->ui_mutex);
        if (app->approval.done) {
            furi_semaphore_release(app->approval.done);
        }
    }
    zf_transport_nfc_signal_worker(app, ZF_NFC_WORKER_EVT_STOP, false);
}

void zf_transport_nfc_send_dispatch_result(ZerofidoApp *app,
                                           const ZfProtocolDispatchRequest *request,
                                           const ZfProtocolDispatchResult *result) {
    ZfNfcTransportState *state = zf_transport_nfc_state(app);
    uint16_t error_status_word = ZF_NFC_SW_INTERNAL_ERROR;

    if (!state || !request || !result) {
        return;
    }

    switch (result->transport_error) {
    case ZF_HID_ERR_INVALID_CMD:
        error_status_word = ZF_NFC_SW_INS_NOT_SUPPORTED;
        break;
    case ZF_HID_ERR_INVALID_LEN:
        error_status_word = ZF_NFC_SW_WRONG_LENGTH;
        break;
    case ZF_HID_ERR_INVALID_PAR:
        error_status_word = ZF_NFC_SW_WRONG_DATA;
        break;
    default:
        error_status_word = ZF_NFC_SW_INTERNAL_ERROR;
        break;
    }

    zf_transport_nfc_store_response(app, state, request->session_id, state->processing_generation,
                                    result->response, result->response_len,
                                    request->protocol == ZfTransportProtocolKindU2f,
                                    result->send_transport_error, error_status_word);
}

bool zf_transport_nfc_wait_for_interaction(ZerofidoApp *app,
                                           ZfTransportSessionId current_session_id,
                                           bool *approved) {
    const ZfNfcTransportState *state = zf_transport_nfc_state(app);

    if (!app || !state || !approved) {
        return false;
    }

    while (true) {
        if (furi_semaphore_acquire(app->approval.done, ZF_KEEPALIVE_INTERVAL_MS) == FuriStatusOk) {
            break;
        }

        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        if (state->stopping || state->processing_cancel_requested ||
            state->processing_session_id != current_session_id) {
            furi_mutex_release(app->ui_mutex);
            return false;
        }
        furi_mutex_release(app->ui_mutex);
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (state->stopping || state->processing_cancel_requested ||
        state->processing_session_id != current_session_id) {
        furi_mutex_release(app->ui_mutex);
        return false;
    }
    *approved = (app->approval.state == ZfApprovalApproved);
    furi_mutex_release(app->ui_mutex);
    return true;
}

void zf_transport_nfc_notify_interaction_changed(ZerofidoApp *app) {
    UNUSED(app);
}

uint8_t zf_transport_nfc_poll_cbor_control(ZerofidoApp *app,
                                           ZfTransportSessionId current_session_id) {
    ZfNfcTransportState *state = zf_transport_nfc_state(app);

    if (!state) {
        return ZF_CTAP_SUCCESS;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (state->stopping ||
        (state->processing_cancel_requested &&
         state->processing_session_id == current_session_id) ||
        state->canceled_session_id == current_session_id) {
        furi_mutex_release(app->ui_mutex);
        return ZF_CTAP_ERR_KEEPALIVE_CANCEL;
    }
    furi_mutex_release(app->ui_mutex);
    return ZF_CTAP_SUCCESS;
}

#endif
