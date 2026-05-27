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

#ifndef ZF_NFC_ONLY

#include "usb_hid_worker.h"

#ifndef ZF_HOST_TEST
#include <furi_hal_usb.h>
#endif
#include <furi_hal_usb_hid_u2f.h>
#include <string.h>

#include "usb_hid_session.h"
#include "../u2f/adapter.h"
#include "../zerofido_app_i.h"
#include "../zerofido_crypto.h"
#include "../zerofido_notify.h"
#include "../zerofido_telemetry.h"
#include "../zerofido_ui.h"
#include "../zerofido_ui_i.h"

#ifdef ZF_HOST_TEST
#define ZF_USB_RESTORE_DEFAULT NULL
#else
#define ZF_USB_RESTORE_DEFAULT (&usb_cdc_single)
#endif

#define ZF_WORKER_EVT_STOP (1 << 0)
#define ZF_WORKER_EVT_CONNECT (1 << 1)
#define ZF_WORKER_EVT_DISCONNECT (1 << 2)
#define ZF_WORKER_EVT_REQUEST (1 << 3)
#define ZF_WORKER_EVT_APPROVAL (1 << 4)
#define ZF_WORKER_POLL_MS 5U

/* Reads connection state under the UI mutex because callbacks and UI share it. */
static bool zf_transport_worker_is_connected(const ZerofidoApp *app) {
    bool connected = false;

    if (!app) {
        return false;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    connected = app->transport_connected;
    furi_mutex_release(app->ui_mutex);
    return connected;
}

static uint32_t zf_transport_worker_wait(uint32_t timeout_ms) {
    return furi_thread_flags_wait(ZF_WORKER_EVT_STOP | ZF_WORKER_EVT_CONNECT |
                                      ZF_WORKER_EVT_DISCONNECT | ZF_WORKER_EVT_REQUEST |
                                      ZF_WORKER_EVT_APPROVAL,
                                  FuriFlagWaitAny, timeout_ms);
}

static void zf_transport_signal_worker(ZerofidoApp *app, uint32_t flags) {
    if (!app || !app->worker_thread) {
        return;
    }

    FuriThreadId id = furi_thread_get_id(app->worker_thread);
    if (id) {
        furi_thread_flags_set(id, flags);
    }
}

/* Converts Flipper HID callbacks into worker-thread flags. */
static void zf_transport_event_callback(HidU2fEvent ev, void *context) {
    ZerofidoApp *app = context;

    furi_assert(app);
    if (!app->worker_thread) {
        return;
    }

    switch (ev) {
    case HidU2fConnected:
        zf_transport_signal_worker(app, ZF_WORKER_EVT_CONNECT);
        break;
    case HidU2fDisconnected:
        zf_transport_signal_worker(app, ZF_WORKER_EVT_DISCONNECT);
        break;
    case HidU2fRequest:
        zf_transport_signal_worker(app, ZF_WORKER_EVT_REQUEST);
        break;
    }
}

/* Claims the U2F HID USB interface while remembering what to restore on exit. */
static bool zf_transport_enable_usb(ZerofidoApp *app) {
    FuriHalUsbInterface *current_usb = NULL;

#ifndef ZF_HOST_TEST
    furi_hal_usb_unlock();
#endif
    furi_hal_hid_u2f_set_callback(NULL, NULL);
    current_usb = furi_hal_usb_get_config();
    app->previous_usb = current_usb == &usb_hid_u2f ? ZF_USB_RESTORE_DEFAULT : current_usb;
    if (current_usb == &usb_hid_u2f || furi_hal_usb_set_config(&usb_hid_u2f, NULL)) {
        return true;
    }

    zerofido_ui_set_status(app, "USB HID init failed");
    return false;
}

static void zf_transport_restore_usb(ZerofidoApp *app) {
    FuriHalUsbInterface *restore_usb = app->previous_usb;

    furi_hal_hid_u2f_set_callback(NULL, NULL);
    if (!restore_usb && furi_hal_usb_get_config() == &usb_hid_u2f) {
        restore_usb = ZF_USB_RESTORE_DEFAULT;
    }
    if (restore_usb) {
        if (furi_hal_usb_set_config(restore_usb, NULL)) {
            app->previous_usb = NULL;
        } else {
            zerofido_ui_set_status(app, "USB restore failed");
        }
    }
}

static bool zf_transport_stop_requested(const ZfTransportState *transport) {
    return (transport && transport->stopping) ||
           ((furi_thread_flags_get() & ZF_WORKER_EVT_STOP) != 0U);
}

static void zf_transport_worker_hide_interaction_if_needed(ZerofidoApp *app, bool canceled) {
    const ViewDispatcher *dispatcher = NULL;

    if (!canceled) {
        return;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (app->ui_events_enabled) {
        dispatcher = app->view_dispatcher;
    }
    furi_mutex_release(app->ui_mutex);

    if (dispatcher) {
        zerofido_ui_dispatch_custom_event(app, ZfEventHideApproval);
    }
}

static void zf_transport_worker_apply_actions(ZerofidoApp *app, uint32_t actions) {
    bool canceled = false;

    if ((actions & ZF_TRANSPORT_ACTION_CANCEL_PENDING_INTERACTION) == 0) {
        return;
    }

    canceled = zerofido_ui_cancel_pending_interaction(app);
    zf_transport_worker_hide_interaction_if_needed(app, canceled);
}

static void zf_transport_worker_on_connect(ZerofidoApp *app, ZfTransportState *transport) {
    if (zf_transport_worker_is_connected(app)) {
        return;
    }

    zf_transport_session_reset(transport);
    zf_transport_session_expire_lock(transport);
    zf_u2f_adapter_set_connected(app, true);
    zerofido_ui_set_transport_connected(app, true);
}

static void zf_transport_worker_on_disconnect(ZerofidoApp *app, ZfTransportState *transport) {
    bool canceled = zerofido_ui_cancel_pending_interaction(app);

    zf_transport_session_reset(transport);
    zf_transport_session_expire_lock(transport);
    zf_u2f_adapter_set_connected(app, false);
    zerofido_ui_set_transport_connected(app, false);
    zerofido_notify_reset(app);
    zf_transport_worker_hide_interaction_if_needed(app, canceled);
}

static void zf_transport_handle_worker_flags(ZerofidoApp *app, ZfTransportState *transport,
                                             uint32_t flags) {
    if (flags & ZF_WORKER_EVT_CONNECT) {
        zf_transport_worker_on_connect(app, transport);
    }
    if (flags & ZF_WORKER_EVT_DISCONNECT) {
        zf_transport_worker_on_disconnect(app, transport);
    }
}

static bool zf_transport_read_request(uint8_t *packet, size_t *packet_len) {
    *packet_len = furi_hal_hid_u2f_get_request(packet);
    return *packet_len > 0;
}

static void zf_transport_tick(ZfTransportState *transport) {
    zf_transport_session_tick(transport, furi_get_tick());
}

static uint32_t zf_transport_worker_next_timeout(const ZfTransportState *transport) {
    UNUSED(transport);
    /* HID request callbacks can coalesce under fast host traffic, so poll as a fallback. */
    return ZF_WORKER_POLL_MS;
}

/* Processes INIT/CANCEL/ABORT packets while a CTAP command waits for UI. */
static bool zf_transport_drain_processing_control_requests(ZerofidoApp *app,
                                                           ZfTransportState *transport) {
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE];

    while (true) {
        uint32_t actions = 0;
        size_t packet_len = 0;

        if (zf_transport_stop_requested(transport)) {
            transport->processing_cancel_requested = true;
            return false;
        }
        if (!zf_transport_read_request(packet, &packet_len)) {
            return true;
        }

        uint8_t status = zf_transport_session_handle_processing_control(app, transport, packet,
                                                                        packet_len, &actions);
        zf_transport_worker_apply_actions(app, actions);
        if (status != ZF_CTAP_SUCCESS) {
            return false;
        }
    }
}

/* Processes one queued HID transaction, reading continuations only until it completes. */
static void zf_transport_handle_request(ZerofidoApp *app, ZfTransportState *transport,
                                        uint32_t flags, uint8_t *packet) {
    uint32_t actions = 0;
    size_t packet_len = 0;

    if (zf_transport_stop_requested(transport)) {
        transport->processing_cancel_requested = true;
        return;
    }
    if ((flags & ZF_WORKER_EVT_REQUEST) == 0 && !zf_transport_read_request(packet, &packet_len)) {
        return;
    }

    while (true) {
        if (zf_transport_stop_requested(transport)) {
            transport->processing_cancel_requested = true;
            return;
        }
        if (packet_len == 0 && !zf_transport_read_request(packet, &packet_len)) {
            return;
        }

        zf_transport_worker_on_connect(app, transport);
        zf_transport_session_handle_packet(app, transport, packet, packet_len, &actions);
        zf_transport_worker_apply_actions(app, actions);
        packet_len = 0;
        if (!transport->active) {
            return;
        }
    }
}

/* Publishes protocol output using the response CTAPHID command for the request kind. */
void zf_transport_usb_hid_send_dispatch_result(ZerofidoApp *app,
                                               const ZfProtocolDispatchRequest *request,
                                               const ZfProtocolDispatchResult *result) {
    uint8_t response_command = ZF_CTAPHID_ERROR;

    if (!request || !result) {
        return;
    }

    if (result->send_transport_error) {
        zf_transport_session_send_error(request->session_id, result->transport_error);
        zf_app_transport_arena_wipe(app);
        return;
    }

    switch (request->protocol) {
    case ZfTransportProtocolKindPing:
        response_command = ZF_CTAPHID_PING;
        break;
    case ZfTransportProtocolKindU2f:
        response_command = ZF_CTAPHID_MSG;
        break;
    case ZfTransportProtocolKindCtap2:
        response_command = ZF_CTAPHID_CBOR;
        break;
    case ZfTransportProtocolKindWink:
        response_command = ZF_CTAPHID_WINK;
        break;
    default:
        zf_transport_session_send_error(request->session_id, ZF_HID_ERR_INVALID_CMD);
        zf_app_transport_arena_wipe(app);
        return;
    }

    if (result->response_len == 0 && request->protocol != ZfTransportProtocolKindWink) {
        zf_transport_session_send_error(request->session_id, ZF_HID_ERR_OTHER);
        zf_app_transport_arena_wipe(app);
        return;
    }

    zf_transport_session_send_frames(request->session_id, response_command, result->response,
                                     result->response_len);
    zf_app_transport_arena_wipe(app);
}

/* Called by long CTAP handlers to notice cancel traffic without holding the UI mutex. */
uint8_t zf_transport_usb_hid_poll_cbor_control(ZerofidoApp *app,
                                               ZfTransportSessionId current_session_id) {
    ZfTransportState *transport = app ? app->transport_state : NULL;

    if (!transport || !transport->processing || transport->cmd != ZF_CTAPHID_CBOR ||
        transport->cid != current_session_id) {
        return ZF_CTAP_SUCCESS;
    }
    if (zf_transport_stop_requested(transport) || transport->processing_cancel_requested) {
        transport->processing_cancel_requested = true;
        return ZF_CTAP_ERR_KEEPALIVE_CANCEL;
    }

    if ((furi_thread_flags_get() & ZF_WORKER_EVT_REQUEST) != 0) {
        furi_thread_flags_clear(ZF_WORKER_EVT_REQUEST);
    }
    if (!zf_transport_drain_processing_control_requests(app, transport) ||
        zf_transport_stop_requested(transport) || transport->processing_cancel_requested) {
        transport->processing_cancel_requested = true;
        return ZF_CTAP_ERR_KEEPALIVE_CANCEL;
    }
    zf_transport_tick(transport);
    return ZF_CTAP_SUCCESS;
}

/* Waits for user approval while continuing keepalives and cancellation handling. */
bool zf_transport_usb_hid_wait_for_interaction(ZerofidoApp *app,
                                               ZfTransportSessionId current_session_id,
                                               bool *approved) {
    ZfTransportState *transport = app ? app->transport_state : NULL;
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE];
    const bool send_keepalive = transport && transport->cmd == ZF_CTAPHID_CBOR;
    bool sent_keepalive = false;

    if (!transport) {
        return false;
    }

    while (true) {
        if (furi_semaphore_acquire(app->approval.done, 0) == FuriStatusOk) {
            break;
        }

        if (send_keepalive && !sent_keepalive) {
            zf_transport_session_send_frames(current_session_id, ZF_CTAPHID_KEEPALIVE,
                                             (const uint8_t[]){ZF_KEEPALIVE_UPNEEDED}, 1);
            sent_keepalive = true;
        }

        uint32_t flags = zf_transport_worker_wait(ZF_KEEPALIVE_INTERVAL_MS);
        if ((flags & FuriFlagError) != 0) {
            if (flags != FuriFlagErrorTimeout) {
                return false;
            }
            zf_transport_handle_request(app, transport, 0, packet);
            if (zf_transport_stop_requested(transport) || transport->processing_cancel_requested) {
                transport->processing_cancel_requested = true;
                return false;
            }
            if (send_keepalive) {
                zf_transport_session_send_frames(current_session_id, ZF_CTAPHID_KEEPALIVE,
                                                 (const uint8_t[]){ZF_KEEPALIVE_UPNEEDED}, 1);
            }
            zf_transport_tick(transport);
            continue;
        }
        if (flags & ZF_WORKER_EVT_STOP) {
            return false;
        }

        zf_transport_handle_worker_flags(app, transport, flags);
        zf_transport_handle_request(app, transport, flags, packet);
        if (zf_transport_stop_requested(transport) || transport->processing_cancel_requested) {
            transport->processing_cancel_requested = true;
            return false;
        }
        zf_transport_tick(transport);
    }

    if (furi_mutex_acquire(app->ui_mutex, FuriWaitForever) != FuriStatusOk) {
        return false;
    }
    *approved = (app->approval.state == ZfApprovalApproved);
    furi_mutex_release(app->ui_mutex);
    return true;
}

/* Main USB lifecycle: claim interface, process event flags, then scrub buffers on shutdown. */
int32_t zf_transport_usb_hid_worker(void *context) {
    ZerofidoApp *app = context;
    ZfTransportState *transport = &app->transport_state_storage;
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE];

    zf_telemetry_log("usb worker start");
    memset(transport, 0, sizeof(*transport));
    if (!zf_app_transport_arena_acquire(app)) {
        zf_telemetry_log_oom("usb transport arena", ZF_TRANSPORT_ARENA_SIZE);
        zerofido_ui_set_transport_connected(app, false);
        return 0;
    }
    zf_transport_session_attach_arena(transport, app->transport_arena, ZF_MAX_MSG_SIZE);
    if (!zf_transport_enable_usb(app)) {
        zerofido_ui_set_transport_connected(app, false);
        zf_app_transport_arena_release(app);
        return 0;
    }

    furi_hal_hid_u2f_set_callback(zf_transport_event_callback, app);
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->transport_state = transport;
    furi_mutex_release(app->ui_mutex);
    if (furi_hal_hid_u2f_is_connected()) {
        zf_transport_worker_on_connect(app, transport);
    }

    while (true) {
        uint32_t flags = zf_transport_worker_wait(zf_transport_worker_next_timeout(transport));

        if ((flags & FuriFlagError) != 0) {
            if (flags == FuriFlagErrorTimeout) {
                flags = 0;
            } else {
                break;
            }
        }
        if (flags & ZF_WORKER_EVT_STOP) {
            break;
        }

        zf_transport_handle_worker_flags(app, transport, flags);
        zf_transport_handle_request(app, transport, flags, packet);
        if (zf_transport_stop_requested(transport)) {
            break;
        }
        zf_transport_tick(transport);
    }

    zf_transport_worker_on_disconnect(app, transport);
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->transport_state = NULL;
    furi_mutex_release(app->ui_mutex);
    zf_transport_session_reset(transport);
    zf_crypto_secure_zero(packet, sizeof(packet));
    zf_app_transport_arena_release(app);
    zf_transport_restore_usb(app);
    zerofido_notify_reset(app);
    zf_telemetry_log("usb worker stop");
    return 0;
}

void zf_transport_usb_hid_stop(ZerofidoApp *app) {
    ZfTransportState *transport = NULL;
    FuriSemaphore *approval_done = NULL;

    if (!app) {
        return;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    transport = app->transport_state;
    approval_done = app->approval.done;
    if (transport) {
        transport->stopping = true;
        transport->processing_cancel_requested = true;
    }
    furi_mutex_release(app->ui_mutex);
    if (approval_done) {
        furi_semaphore_release(approval_done);
    }
    zf_transport_signal_worker(app, ZF_WORKER_EVT_STOP);
}

void zf_transport_usb_hid_notify_interaction_changed(ZerofidoApp *app) {
    if (!app) {
        return;
    }

    zf_transport_signal_worker(app, ZF_WORKER_EVT_APPROVAL);
}

#endif
