#include "bridge_session.h"

#include "bridge_protocol.h"
#include "usb_transport.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <furi_hal_version.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG                             "FibSession"
#define FIB_CAPABILITY_HTTPS_GET        (1UL << 0)
#define FIB_CAPABILITY_REQUEST_HEADERS  (1UL << 2)
#define FIB_CAPABILITY_RESPONSE_HEADERS (1UL << 3)
#define FIB_CAPABILITY_CANCELLATION     (1UL << 4)
#define FIB_CLIENT_CAPABILITIES \
    (FIB_CAPABILITY_HTTPS_GET | FIB_CAPABILITY_REQUEST_HEADERS | \
     FIB_CAPABILITY_RESPONSE_HEADERS | FIB_CAPABILITY_CANCELLATION)
#define FIB_UNKNOWN_BODY_LENGTH UINT32_MAX
#define FIB_FRAME_TX_TIMEOUT_MS 1000U
#define FIB_ERROR_RATE_LIMIT_MS 250U

typedef enum {
    BridgeSequenceAccepted,
    BridgeSequenceDuplicate,
    BridgeSequenceGap,
} BridgeSequenceResult;

struct BridgeSession {
    UsbTransport* transport;
    FuriMutex* state_mutex;
    FuriMutex* parser_mutex;
    FuriMutex* tx_frame_mutex;
    FibParser parser;
    FibFrame tx_frame;
    uint8_t tx_encoded[FIB_MAX_FRAME_SIZE];

    BridgeSessionUpdateCallback update_callback;
    void* update_context;
    BridgeSessionBodyCallback body_callback;
    void* body_context;
    bool closing;

    BridgeSessionState state;
    BridgePermission permission;
    bool usb_connected;
    bool helper_present;
    uint8_t selected_major;
    uint8_t selected_minor;
    uint32_t negotiated_capabilities;
    uint16_t negotiated_payload;
    uint32_t negotiated_response_bytes;
    uint64_t client_nonce;
    uint64_t server_nonce;
    uint32_t next_control_sequence;
    uint32_t expected_control_sequence;
    uint32_t next_request_id;

    bool ping_pending;
    uint64_t ping_token;
    uint32_t ping_started_tick;
    uint32_t hello_sent_tick;
    uint32_t last_activity_tick;
    uint32_t parser_last_progress_tick;
    uint32_t last_error_sent_tick;
    bool error_sent_once;

    bool active_request;
    uint32_t active_request_id;
    uint32_t request_timeout_ms;
    uint32_t request_started_tick;
    uint32_t next_request_sequence;

    bool response_started;
    bool response_body_started;
    uint32_t expected_response_sequence;
    uint8_t response_headers_expected;
    uint8_t response_headers_received;
    uint16_t http_status;
    uint32_t declared_response_bytes;
    uint32_t response_bytes;
    bool response_truncated;
    size_t preview_length;
    char preview[FIB_RESPONSE_PREVIEW_SIZE + 1U];

    char device_name[33];
    char uid_suffix[9];
    char app_version[17];
    char detail[FIB_MAX_ERROR_DETAIL + 1U];
};

static bool bridge_ticks_elapsed(uint32_t now, uint32_t then, uint32_t milliseconds) {
    return (uint32_t)(now - then) >= furi_ms_to_ticks(milliseconds);
}

static uint32_t bridge_ticks_until(uint32_t deadline) {
    const int32_t remaining = (int32_t)(deadline - furi_get_tick());
    return (remaining > 0) ? (uint32_t)remaining : 0U;
}

static void bridge_session_set_detail_locked(BridgeSession* session, const char* detail) {
    snprintf(session->detail, sizeof(session->detail), "%s", detail ? detail : "");
}

static bool bridge_session_permission_is_allowed(BridgePermission permission) {
    return permission == BridgePermissionAllowedOnce ||
           permission == BridgePermissionAllowedAlways;
}

static bool bridge_session_can_cancel_locked(const BridgeSession* session) {
    return session->selected_major != 0U &&
           (session->negotiated_capabilities & FIB_CAPABILITY_CANCELLATION) != 0U;
}

static void bridge_session_restore_permission_state_locked(
    BridgeSession* session,
    const char* allowed_detail) {
    if(bridge_session_permission_is_allowed(session->permission)) {
        session->state = BridgeSessionStateReady;
        bridge_session_set_detail_locked(session, allowed_detail);
    } else if(session->permission == BridgePermissionDenied) {
        session->state = BridgeSessionStatePermissionDenied;
        bridge_session_set_detail_locked(session, "Internet permission denied");
    } else {
        session->state = BridgeSessionStatePermissionPending;
        bridge_session_set_detail_locked(session, "Waiting for host permission");
    }
}

static void bridge_session_notify(BridgeSession* session) {
    BridgeSessionUpdateCallback callback = NULL;
    void* context = NULL;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    if(!session->closing) {
        callback = session->update_callback;
        context = session->update_context;
    }
    furi_mutex_release(session->state_mutex);
    if(callback) callback(context);
}

static void
    bridge_session_set_state(BridgeSession* session, BridgeSessionState state, const char* detail) {
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->state = state;
    bridge_session_set_detail_locked(session, detail);
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
}

static void bridge_session_note_activity(BridgeSession* session) {
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->last_activity_tick = furi_get_tick();
    furi_mutex_release(session->state_mutex);
}

static uint32_t bridge_session_take_control_sequence(BridgeSession* session) {
    uint32_t sequence;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    sequence = session->next_control_sequence++;
    furi_mutex_release(session->state_mutex);
    return sequence;
}

static bool bridge_session_send_frame(
    BridgeSession* session,
    FibMessageType type,
    uint16_t flags,
    uint32_t request_id,
    uint32_t sequence,
    const uint8_t* payload,
    size_t payload_length) {
    if(payload_length > FIB_MAX_FRAME_PAYLOAD || (!payload && payload_length != 0U)) {
        return false;
    }
    const uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(FIB_FRAME_TX_TIMEOUT_MS);
    const uint32_t frame_wait = bridge_ticks_until(deadline);
    if(frame_wait == 0U ||
       furi_mutex_acquire(session->tx_frame_mutex, frame_wait) != FuriStatusOk) {
        return false;
    }

    memset(&session->tx_frame, 0, sizeof(session->tx_frame));
    session->tx_frame.major = FIB_PROTOCOL_MAJOR;
    session->tx_frame.minor = FIB_PROTOCOL_MINOR;
    session->tx_frame.type = type;
    session->tx_frame.flags = flags;
    session->tx_frame.request_id = request_id;
    session->tx_frame.sequence = sequence;
    session->tx_frame.payload_length = (uint32_t)payload_length;
    if(payload_length != 0U) {
        memcpy(session->tx_frame.payload, payload, payload_length);
    }

    size_t encoded_length = 0U;
    const bool encoded = fib_frame_encode(
        &session->tx_frame, session->tx_encoded, sizeof(session->tx_encoded), &encoded_length);
    const bool sent = encoded &&
                      usb_transport_send_until(
                          session->transport, session->tx_encoded, encoded_length, deadline);
    furi_mutex_release(session->tx_frame_mutex);
    if(sent) bridge_session_note_activity(session);
    return sent;
}

static void bridge_session_copy_wire_text(
    char* destination,
    size_t destination_size,
    const uint8_t* source,
    size_t source_length) {
    if(destination_size == 0U) return;
    const size_t copy_length = (source_length < destination_size - 1U) ? source_length :
                                                                         destination_size - 1U;
    size_t written = 0U;
    for(size_t index = 0U; index < copy_length; ++index) {
        const uint8_t value = source[index];
        if(value >= 0x20U && value <= 0x7EU) {
            destination[written++] = (char)value;
        } else if(value == '\n' || value == '\r' || value == '\t') {
            destination[written++] = ' ';
        } else {
            destination[written++] = '?';
        }
    }
    destination[written] = '\0';
}

static bool bridge_session_send_error(
    BridgeSession* session,
    FibErrorCode code,
    uint8_t scope,
    FibMessageType offending_type,
    uint32_t request_id,
    const char* detail) {
    uint8_t payload[6U + FIB_MAX_ERROR_DETAIL];
    size_t maximum_detail = FIB_MAX_ERROR_DETAIL;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    if(session->selected_major != 0U) {
        if(session->negotiated_payload < 6U) {
            maximum_detail = 0U;
        } else if(maximum_detail > session->negotiated_payload - 6U) {
            maximum_detail = session->negotiated_payload - 6U;
        }
    }
    furi_mutex_release(session->state_mutex);
    size_t detail_length = detail ? strlen(detail) : 0U;
    if(detail_length > maximum_detail) detail_length = maximum_detail;
    fib_write_u16_le(payload, (uint16_t)code);
    payload[2] = scope;
    payload[3] = (uint8_t)offending_type;
    fib_write_u16_le(payload + 4U, (uint16_t)detail_length);
    if(detail_length != 0U) memcpy(payload + 6U, detail, detail_length);
    return bridge_session_send_frame(
        session,
        FibMessageError,
        0U,
        request_id,
        bridge_session_take_control_sequence(session),
        payload,
        6U + detail_length);
}

static bool bridge_session_send_cancel(
    BridgeSession* session,
    uint32_t request_id,
    uint32_t sequence,
    uint8_t reason) {
    return bridge_session_send_frame(
        session, FibMessageCancel, FibFlagFinal, request_id, sequence, &reason, 1U);
}

static bool bridge_session_detach_active_request_locked(
    BridgeSession* session,
    uint32_t* request_id,
    uint32_t* sequence) {
    if(!session->active_request) return false;
    if(request_id) *request_id = session->active_request_id;
    const bool can_send = bridge_session_can_cancel_locked(session);
    if(sequence) {
        *sequence = can_send ? session->next_request_sequence++ : 0U;
    }
    session->active_request = false;
    return can_send;
}

static bool bridge_session_send_hello(BridgeSession* session) {
    uint8_t payload[128];
    size_t used = 0U;
    uint64_t nonce;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    nonce = session->client_nonce;
    session->hello_sent_tick = furi_get_tick();
    furi_mutex_release(session->state_mutex);

    payload[used++] = FIB_PROTOCOL_MAJOR;
    payload[used++] = FIB_PROTOCOL_MINOR;
    payload[used++] = FIB_PROTOCOL_MAJOR;
    payload[used++] = FIB_PROTOCOL_MINOR;
    fib_write_u32_le(payload + used, FIB_CLIENT_CAPABILITIES);
    used += 4U;
    fib_write_u16_le(payload + used, FIB_MAX_FRAME_PAYLOAD);
    used += 2U;
    fib_write_u32_le(payload + used, FIB_MAX_RESPONSE_SIZE);
    used += 4U;
    fib_write_u64_le(payload + used, nonce);
    used += 8U;

    const char* model = furi_hal_version_get_model_name();
    if(!model || model[0] == '\0') model = "Flipper Zero";
    size_t model_length = strlen(model);
    if(model_length > 16U) model_length = 16U;
    payload[used++] = (uint8_t)model_length;
    memcpy(payload + used, model, model_length);
    used += model_length;

    const char* name = furi_hal_version_get_name_ptr();
    if(!name || name[0] == '\0') name = "Unknown";
    size_t name_length = strlen(name);
    if(name_length > 32U) name_length = 32U;
    payload[used++] = (uint8_t)name_length;
    memcpy(payload + used, name, name_length);
    used += name_length;

    const uint8_t* uid = furi_hal_version_uid();
    size_t uid_length = furi_hal_version_uid_size();
    if(uid_length > 16U) uid_length = 16U;
    payload[used++] = 1U;
    payload[used++] = (uint8_t)uid_length;
    if(uid && uid_length != 0U) {
        memcpy(payload + used, uid, uid_length);
        used += uid_length;
    }

    size_t version_length = strlen(session->app_version);
    if(version_length > 16U) version_length = 16U;
    payload[used++] = (uint8_t)version_length;
    memcpy(payload + used, session->app_version, version_length);
    used += version_length;

    return bridge_session_send_frame(session, FibMessageHello, 0U, 0U, 0U, payload, used);
}

static BridgeSequenceResult
    bridge_session_accept_control_sequence(BridgeSession* session, uint32_t sequence) {
    BridgeSequenceResult result;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    if(sequence < session->expected_control_sequence) {
        result = BridgeSequenceDuplicate;
    } else if(sequence > session->expected_control_sequence) {
        result = BridgeSequenceGap;
    } else {
        ++session->expected_control_sequence;
        result = BridgeSequenceAccepted;
    }
    furi_mutex_release(session->state_mutex);
    return result;
}

static bool bridge_session_check_control_sequence(BridgeSession* session, const FibFrame* frame) {
    const BridgeSequenceResult result =
        bridge_session_accept_control_sequence(session, frame->sequence);
    if(result == BridgeSequenceAccepted) return true;
    bridge_session_send_error(
        session,
        (result == BridgeSequenceDuplicate) ? FibErrorDuplicateSequence : FibErrorSequenceGap,
        0U,
        frame->type,
        0U,
        (result == BridgeSequenceDuplicate) ? "duplicate control sequence" :
                                              "control sequence gap");
    return false;
}

static bool bridge_session_consume_control_sequence_silently(
    BridgeSession* session,
    const FibFrame* frame) {
    return bridge_session_accept_control_sequence(session, frame->sequence) ==
           BridgeSequenceAccepted;
}

static void bridge_session_handle_hello_ack(BridgeSession* session, const FibFrame* frame) {
    if(frame->request_id != 0U || frame->sequence != 0U || frame->payload_length != 28U) {
        bridge_session_send_error(
            session, FibErrorInvalidRequest, 0U, frame->type, 0U, "invalid HELLO_ACK");
        return;
    }

    const uint8_t selected_major = frame->payload[0];
    const uint8_t selected_minor = frame->payload[1];
    const uint32_t capabilities = fib_read_u32_le(frame->payload + 2U);
    const uint16_t maximum_payload = fib_read_u16_le(frame->payload + 6U);
    const uint32_t maximum_response = fib_read_u32_le(frame->payload + 8U);
    const uint64_t echoed_nonce = fib_read_u64_le(frame->payload + 12U);
    const uint64_t server_nonce = fib_read_u64_le(frame->payload + 20U);

    bool valid_nonce;
    bool already_handshaken;
    bool duplicate_ack;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    valid_nonce = echoed_nonce == session->client_nonce;
    already_handshaken = session->selected_major != 0U;
    duplicate_ack =
        already_handshaken && valid_nonce && selected_major == session->selected_major &&
        selected_minor == session->selected_minor && server_nonce == session->server_nonce;
    furi_mutex_release(session->state_mutex);
    if(duplicate_ack) return;
    if(already_handshaken) {
        bridge_session_send_error(
            session, FibErrorInvalidState, 0U, frame->type, 0U, "unexpected HELLO_ACK");
        return;
    }
    if(selected_major != FIB_PROTOCOL_MAJOR || selected_minor != FIB_PROTOCOL_MINOR ||
       maximum_payload < FIB_MIN_NEGOTIATED_PAYLOAD || maximum_payload > FIB_MAX_FRAME_PAYLOAD ||
       maximum_response == 0U || (capabilities & FIB_CAPABILITY_HTTPS_GET) == 0U ||
       (capabilities & FIB_CAPABILITY_CANCELLATION) == 0U || !valid_nonce) {
        bridge_session_send_error(
            session,
            (selected_major != FIB_PROTOCOL_MAJOR) ? FibErrorUnsupportedVersion :
                                                     FibErrorInvalidRequest,
            0U,
            frame->type,
            0U,
            "HELLO_ACK negotiation failed");
        bridge_session_set_state(session, BridgeSessionStateError, "Protocol negotiation failed");
        return;
    }

    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->selected_major = selected_major;
    session->selected_minor = selected_minor;
    session->negotiated_capabilities = capabilities & FIB_CLIENT_CAPABILITIES;
    session->negotiated_payload = maximum_payload;
    session->negotiated_response_bytes =
        (maximum_response < FIB_MAX_RESPONSE_SIZE) ? maximum_response : FIB_MAX_RESPONSE_SIZE;
    session->server_nonce = server_nonce;
    session->expected_control_sequence = 1U;
    session->state = BridgeSessionStatePermissionPending;
    session->permission = BridgePermissionPending;
    bridge_session_set_detail_locked(session, "Waiting for host permission status");
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
}

static void
    bridge_session_handle_permission_required(BridgeSession* session, const FibFrame* frame) {
    if(frame->request_id != 0U || frame->payload_length != 1U) {
        bridge_session_send_error(
            session, FibErrorInvalidRequest, 0U, frame->type, 0U, "invalid permission message");
        return;
    }
    if(!bridge_session_check_control_sequence(session, frame)) {
        return;
    }
    if(frame->payload[0] > 1U) {
        bridge_session_send_error(
            session, FibErrorInvalidRequest, 0U, frame->type, 0U, "invalid permission reason");
        return;
    }
    uint32_t request_id = 0U;
    uint32_t request_sequence = 0U;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    const bool send_cancel =
        bridge_session_detach_active_request_locked(session, &request_id, &request_sequence);
    session->ping_pending = false;
    session->permission = BridgePermissionPending;
    session->state = BridgeSessionStatePermissionPending;
    bridge_session_set_detail_locked(session, "Waiting for host permission");
    furi_mutex_release(session->state_mutex);
    if(send_cancel) {
        bridge_session_send_cancel(session, request_id, request_sequence, 3U);
    }
    bridge_session_notify(session);
}

static void
    bridge_session_handle_permission_status(BridgeSession* session, const FibFrame* frame) {
    if(frame->request_id != 0U || frame->payload_length != 2U) {
        bridge_session_send_error(
            session, FibErrorInvalidRequest, 0U, frame->type, 0U, "invalid permission status");
        return;
    }
    if(!bridge_session_check_control_sequence(session, frame)) {
        return;
    }

    const uint8_t state = frame->payload[0];
    const uint8_t reason = frame->payload[1];
    if(state > 2U || reason > 2U) {
        bridge_session_send_error(
            session, FibErrorInvalidRequest, 0U, frame->type, 0U, "invalid permission value");
        bridge_session_set_state(session, BridgeSessionStateError, "Invalid permission message");
        return;
    }
    uint32_t request_id = 0U;
    uint32_t request_sequence = 0U;
    bool send_cancel = false;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->ping_pending = false;
    if(state == 1U || state == 2U) {
        session->permission = (state == 1U) ? BridgePermissionAllowedOnce :
                                              BridgePermissionAllowedAlways;
        if(!session->active_request) {
            bridge_session_restore_permission_state_locked(session, "Internet access ready");
        }
    } else {
        send_cancel =
            bridge_session_detach_active_request_locked(session, &request_id, &request_sequence);
        session->permission = BridgePermissionDenied;
        session->state = BridgeSessionStatePermissionDenied;
        bridge_session_set_detail_locked(session, "Internet permission denied");
    }
    furi_mutex_release(session->state_mutex);
    if(send_cancel) {
        bridge_session_send_cancel(session, request_id, request_sequence, 3U);
    }
    bridge_session_notify(session);
}

static void bridge_session_handle_ping(BridgeSession* session, const FibFrame* frame) {
    if(frame->request_id != 0U || frame->payload_length != 8U ||
       !bridge_session_check_control_sequence(session, frame)) {
        return;
    }
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    const bool allowed = bridge_session_permission_is_allowed(session->permission);
    furi_mutex_release(session->state_mutex);
    if(!allowed) {
        bridge_session_send_error(
            session, FibErrorInvalidState, 0U, frame->type, 0U, "permission not granted");
        return;
    }
    bridge_session_send_frame(
        session,
        FibMessagePong,
        0U,
        0U,
        bridge_session_take_control_sequence(session),
        frame->payload,
        frame->payload_length);
}

static void bridge_session_handle_pong(BridgeSession* session, const FibFrame* frame) {
    if(frame->request_id != 0U || frame->payload_length != 8U ||
       !bridge_session_check_control_sequence(session, frame)) {
        return;
    }

    const uint64_t token = fib_read_u64_le(frame->payload);
    bool matched = false;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    if(session->ping_pending && token == session->ping_token) {
        session->ping_pending = false;
        bridge_session_restore_permission_state_locked(session, "Connection test successful");
        matched = true;
    }
    furi_mutex_release(session->state_mutex);
    if(matched) bridge_session_notify(session);
}

static FibErrorCode
    bridge_session_check_response_sequence_locked(BridgeSession* session, const FibFrame* frame) {
    if(!session->active_request || frame->request_id != session->active_request_id ||
       !session->response_started) {
        return FibErrorInvalidState;
    }
    if(frame->sequence < session->expected_response_sequence) {
        return FibErrorDuplicateSequence;
    }
    if(frame->sequence > session->expected_response_sequence) {
        session->active_request = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(session, "Invalid response sequence");
        return FibErrorSequenceGap;
    }
    ++session->expected_response_sequence;
    return 0;
}

static void bridge_session_report_response_error(
    BridgeSession* session,
    const FibFrame* frame,
    FibErrorCode error) {
    const char* detail = "invalid response state";
    if(error == FibErrorDuplicateSequence) detail = "duplicate response sequence";
    if(error == FibErrorSequenceGap) detail = "response sequence gap";
    if(error == FibErrorResponseTooLarge) detail = "response too large";
    bridge_session_send_error(session, error, 1U, frame->type, frame->request_id, detail);
    bridge_session_notify(session);
}

static void bridge_session_handle_response_start(BridgeSession* session, const FibFrame* frame) {
    FibErrorCode error = 0;
    bool belongs_to_active_request = false;
    if(frame->payload_length != 8U || frame->request_id == 0U || frame->payload[3] != 0U ||
       frame->payload[2] > FIB_MAX_REQUEST_HEADERS) {
        error = FibErrorInvalidRequest;
    }

    const uint16_t status = (frame->payload_length >= 2U) ? fib_read_u16_le(frame->payload) : 0U;
    const uint32_t declared =
        (frame->payload_length == 8U) ? fib_read_u32_le(frame->payload + 4U) : 0U;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    belongs_to_active_request = session->active_request &&
                                frame->request_id == session->active_request_id;
    if(belongs_to_active_request && session->response_started &&
       frame->sequence < session->expected_response_sequence) {
        error = FibErrorDuplicateSequence;
    }
    const uint32_t response_limit = session->negotiated_response_bytes != 0U ?
                                        session->negotiated_response_bytes :
                                        FIB_MAX_RESPONSE_SIZE;
    if(error == 0U && declared != FIB_UNKNOWN_BODY_LENGTH && declared > response_limit) {
        error = FibErrorResponseTooLarge;
    }
    if(error == 0U) {
        if(!belongs_to_active_request || session->response_started) {
            error = FibErrorInvalidState;
        } else if(frame->sequence != 0U) {
            error = FibErrorSequenceGap;
        }
    }
    if(error == 0U) {
        session->response_started = true;
        session->response_body_started = false;
        session->expected_response_sequence = 1U;
        session->response_headers_expected = frame->payload[2];
        session->response_headers_received = 0U;
        session->http_status = status;
        session->declared_response_bytes = declared;
        session->response_bytes = 0U;
        session->preview_length = 0U;
        session->preview[0] = '\0';
        session->state = BridgeSessionStateReceivingResponse;
        bridge_session_set_detail_locked(session, "Receiving response");
    } else if(error != FibErrorDuplicateSequence && belongs_to_active_request) {
        session->active_request = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(
            session,
            (error == FibErrorResponseTooLarge) ? "Response size limit exceeded" :
                                                  "Invalid response start");
    }
    furi_mutex_release(session->state_mutex);

    if(error != 0U) {
        bridge_session_report_response_error(session, frame, error);
    } else {
        bridge_session_notify(session);
    }
}

static bool bridge_session_is_http_token_byte(uint8_t value) {
    if((value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
       (value >= 'a' && value <= 'z')) {
        return true;
    }
    switch(value) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
        return true;
    default:
        return false;
    }
}

static bool bridge_session_valid_wire_utf8(const uint8_t* bytes, size_t length, bool allow_tab) {
    size_t index = 0U;
    while(index < length) {
        const uint8_t first = bytes[index];
        if(first <= 0x7FU) {
            if((first < 0x20U && !(allow_tab && first == '\t')) || first == 0x7FU) return false;
            ++index;
        } else if(first >= 0xC2U && first <= 0xDFU) {
            if(index + 1U >= length || (bytes[index + 1U] & 0xC0U) != 0x80U) return false;
            index += 2U;
        } else if(first >= 0xE0U && first <= 0xEFU) {
            if(index + 2U >= length || (bytes[index + 2U] & 0xC0U) != 0x80U) return false;
            const uint8_t second = bytes[index + 1U];
            if((first == 0xE0U && (second < 0xA0U || second > 0xBFU)) ||
               (first == 0xEDU && (second < 0x80U || second > 0x9FU)) ||
               (first != 0xE0U && first != 0xEDU && (second & 0xC0U) != 0x80U)) {
                return false;
            }
            index += 3U;
        } else if(first >= 0xF0U && first <= 0xF4U) {
            if(index + 3U >= length || (bytes[index + 2U] & 0xC0U) != 0x80U ||
               (bytes[index + 3U] & 0xC0U) != 0x80U) {
                return false;
            }
            const uint8_t second = bytes[index + 1U];
            if((first == 0xF0U && (second < 0x90U || second > 0xBFU)) ||
               (first == 0xF4U && (second < 0x80U || second > 0x8FU)) ||
               (first != 0xF0U && first != 0xF4U && (second & 0xC0U) != 0x80U)) {
                return false;
            }
            index += 4U;
        } else {
            return false;
        }
    }
    return true;
}

static bool bridge_session_valid_header_payload(const FibFrame* frame) {
    if(frame->payload_length < 3U) return false;
    const size_t name_length = frame->payload[0];
    const size_t value_length = fib_read_u16_le(frame->payload + 1U);
    if(name_length == 0U || name_length > FIB_MAX_HEADER_NAME_LENGTH ||
       value_length > FIB_MAX_HEADER_VALUE_LENGTH) {
        return false;
    }
    if(3U + name_length + value_length != frame->payload_length) return false;

    for(size_t index = 0U; index < name_length; ++index) {
        if(!bridge_session_is_http_token_byte(frame->payload[3U + index])) return false;
    }
    return bridge_session_valid_wire_utf8(frame->payload + 3U + name_length, value_length, true);
}

static void bridge_session_handle_response_header(BridgeSession* session, const FibFrame* frame) {
    FibErrorCode error = 0;
    bool belongs_to_active_request = false;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    belongs_to_active_request = session->active_request &&
                                frame->request_id == session->active_request_id;
    error = bridge_session_check_response_sequence_locked(session, frame);
    if(error == 0U && (session->response_body_started ||
                       session->response_headers_received >= session->response_headers_expected ||
                       !bridge_session_valid_header_payload(frame))) {
        error = FibErrorInvalidRequest;
    }
    if(error == 0U) ++session->response_headers_received;
    if(error != 0U && error != FibErrorDuplicateSequence && belongs_to_active_request) {
        session->active_request = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(session, "Invalid response header");
    }
    furi_mutex_release(session->state_mutex);
    if(error != 0U) bridge_session_report_response_error(session, frame, error);
}

static void bridge_session_append_preview_locked(
    BridgeSession* session,
    const uint8_t* bytes,
    size_t length) {
    for(size_t index = 0U; index < length && session->preview_length < FIB_RESPONSE_PREVIEW_SIZE;
        ++index) {
        const uint8_t value = bytes[index];
        if(value == '\n' || value == '\r' || value == '\t') {
            /* Feature parsers use tabs as bounded field separators. UI renderers
             * consume their parsed output, so retaining a tab here is safe. */
            session->preview[session->preview_length++] = (char)value;
        } else if(value >= 0x20U) {
            /* Keep UTF-8 intact so feature-specific renderers can transliterate it. */
            session->preview[session->preview_length++] = (char)value;
        } else {
            session->preview[session->preview_length++] = ' ';
        }
    }
    session->preview[session->preview_length] = '\0';
}

static void bridge_session_handle_response_body(BridgeSession* session, const FibFrame* frame) {
    FibErrorCode error = 0;
    bool belongs_to_active_request = false;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    belongs_to_active_request = session->active_request &&
                                frame->request_id == session->active_request_id;
    const uint32_t response_limit = session->negotiated_response_bytes != 0U ?
                                        session->negotiated_response_bytes :
                                        FIB_MAX_RESPONSE_SIZE;
    error = bridge_session_check_response_sequence_locked(session, frame);
    if(error == 0U && (session->response_headers_received != session->response_headers_expected ||
                       frame->payload_length > FIB_RESPONSE_CHUNK_SIZE ||
                       frame->payload_length > response_limit ||
                       session->response_bytes > response_limit - frame->payload_length)) {
        error = (frame->payload_length > response_limit ||
                 session->response_bytes > response_limit - frame->payload_length) ?
                    FibErrorResponseTooLarge :
                    FibErrorInvalidRequest;
    }
    if(error == 0U) {
        session->response_body_started = true;
        session->response_bytes += frame->payload_length;
        /* For streaming responses the request timeout is an inactivity timeout,
         * not a hard lifetime cap. Any valid body chunk proves forward progress. */
        session->request_started_tick = furi_get_tick();
        bridge_session_append_preview_locked(session, frame->payload, frame->payload_length);
        if(session->declared_response_bytes != FIB_UNKNOWN_BODY_LENGTH &&
           session->response_bytes > session->declared_response_bytes) {
            error = FibErrorInvalidRequest;
        }
    }
    if(error != 0U && error != FibErrorDuplicateSequence && belongs_to_active_request) {
        session->active_request = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(
            session,
            (error == FibErrorResponseTooLarge) ? "Response size limit exceeded" :
                                                  "Invalid response data");
    }
    BridgeSessionBodyCallback body_callback = error == 0U ? session->body_callback : NULL;
    void* body_context = session->body_context;
    furi_mutex_release(session->state_mutex);

    if(error != 0U) {
        bridge_session_report_response_error(session, frame, error);
    } else {
        if(body_callback && !body_callback(body_context, frame->payload, frame->payload_length)) {
            bridge_session_cancel(session);
        }
        /* Text responses update the UI for each chunk. A radio body callback
         * receives roughly fifty frames per second, so posting a GUI event
         * here would flood the dispatcher. The radio screen refreshes from
         * its bounded 500 ms tick instead. */
        if(!body_callback) bridge_session_notify(session);
    }
}

static void bridge_session_handle_response_end(BridgeSession* session, const FibFrame* frame) {
    FibErrorCode error = 0;
    bool belongs_to_active_request = false;
    const uint8_t result = (frame->payload_length == 5U) ? frame->payload[0] : 0xFFU;
    const uint32_t bytes_sent =
        (frame->payload_length == 5U) ? fib_read_u32_le(frame->payload + 1U) : 0U;

    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    belongs_to_active_request = session->active_request &&
                                frame->request_id == session->active_request_id;
    error = bridge_session_check_response_sequence_locked(session, frame);
    if(error == 0U && (frame->payload_length != 5U || (frame->flags & FibFlagFinal) == 0U ||
                       result > 2U || bytes_sent != session->response_bytes ||
                       session->response_headers_received != session->response_headers_expected)) {
        error = FibErrorInvalidRequest;
    }
    if(error == 0U && result == 0U &&
       session->declared_response_bytes != FIB_UNKNOWN_BODY_LENGTH &&
       session->declared_response_bytes != session->response_bytes) {
        error = FibErrorInvalidRequest;
    }

    if(error == 0U) {
        session->active_request = false;
        session->response_truncated = result == 1U || (frame->flags & FibFlagTruncated) != 0U;
        if(result == 2U) {
            session->state = BridgeSessionStateCancelled;
            bridge_session_set_detail_locked(session, "Request cancelled");
        } else {
            session->state = BridgeSessionStateComplete;
            bridge_session_set_detail_locked(
                session,
                session->response_truncated ? "Response truncated at limit" : "Request complete");
        }
    } else if(error != FibErrorDuplicateSequence && belongs_to_active_request) {
        session->active_request = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(session, "Invalid response end");
    }
    furi_mutex_release(session->state_mutex);

    if(error != 0U) bridge_session_report_response_error(session, frame, error);
    bridge_session_notify(session);
}

static const char* bridge_session_error_text(FibErrorCode error) {
    switch(error) {
    case FibErrorPermissionDenied:
        return "Internet permission denied";
    case FibErrorSecurityBlocked:
        return "Request blocked for security";
    case FibErrorTimeout:
        return "Request timed out";
    case FibErrorCancelled:
        return "Request cancelled";
    case FibErrorResponseTooLarge:
        return "Response too large";
    case FibErrorTransportLost:
        return "USB disconnected";
    case FibErrorNetworkFailure:
        return "Network request failed";
    case FibErrorUnsupportedVersion:
        return "Incompatible protocol version";
    default:
        return "Bridge error";
    }
}

static bool bridge_session_error_prefers_local_text(FibErrorCode error) {
    switch(error) {
    case FibErrorPermissionDenied:
    case FibErrorSecurityBlocked:
    case FibErrorTimeout:
    case FibErrorCancelled:
    case FibErrorResponseTooLarge:
    case FibErrorTransportLost:
    case FibErrorNetworkFailure:
    case FibErrorUnsupportedVersion:
        return true;
    default:
        return false;
    }
}

static void bridge_session_handle_error(BridgeSession* session, const FibFrame* frame) {
    if(!bridge_session_consume_control_sequence_silently(session, frame)) return;
    if(frame->payload_length < 6U) return;
    const FibErrorCode code = (FibErrorCode)fib_read_u16_le(frame->payload);
    const uint8_t scope = frame->payload[2];
    const size_t detail_length = fib_read_u16_le(frame->payload + 4U);
    if(detail_length > FIB_MAX_ERROR_DETAIL || 6U + detail_length != frame->payload_length) {
        return;
    }
    if(scope > 1U || (scope == 0U && frame->request_id != 0U) ||
       (scope == 1U && frame->request_id == 0U)) {
        return;
    }
    if(!bridge_session_valid_wire_utf8(frame->payload + 6U, detail_length, false)) return;

    char remote_detail[FIB_MAX_ERROR_DETAIL + 1U];
    bridge_session_copy_wire_text(
        remote_detail, sizeof(remote_detail), frame->payload + 6U, detail_length);
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    if(scope == 1U && frame->request_id != 0U && frame->request_id != session->active_request_id) {
        furi_mutex_release(session->state_mutex);
        return;
    }
    session->active_request = false;
    session->ping_pending = false;
    if(code == FibErrorPermissionDenied) {
        session->permission = BridgePermissionDenied;
        session->state = BridgeSessionStatePermissionDenied;
    } else if(code == FibErrorTimeout) {
        session->state = BridgeSessionStateTimedOut;
    } else if(code == FibErrorCancelled) {
        session->state = BridgeSessionStateCancelled;
    } else {
        session->state = BridgeSessionStateError;
    }
    const char* display_detail = bridge_session_error_text(code);
    if(!bridge_session_error_prefers_local_text(code) && remote_detail[0] != '\0') {
        display_detail = remote_detail;
    }
    bridge_session_set_detail_locked(session, display_detail);
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
}

static FibErrorCode
    bridge_session_check_cancel_sequence_locked(BridgeSession* session, const FibFrame* frame) {
    uint32_t expected_sequence = 0U;
    if(session->active_request && frame->request_id == session->active_request_id) {
        expected_sequence = session->response_started ? session->expected_response_sequence : 0U;
    } else if(
        !session->active_request && session->state == BridgeSessionStateCancelled &&
        frame->request_id == session->active_request_id &&
        frame->sequence < session->expected_response_sequence) {
        return FibErrorDuplicateSequence;
    } else {
        return FibErrorInvalidState;
    }

    if(frame->sequence < expected_sequence) return FibErrorDuplicateSequence;
    if(frame->sequence > expected_sequence) return FibErrorSequenceGap;
    session->expected_response_sequence = expected_sequence + 1U;
    return 0;
}

static void bridge_session_handle_cancel(BridgeSession* session, const FibFrame* frame) {
    if(frame->request_id == 0U) {
        bridge_session_send_error(
            session, FibErrorInvalidRequest, 0U, frame->type, 0U, "cancel requires request id");
        return;
    }
    if(frame->payload_length != 1U || frame->payload[0] > 3U) {
        bridge_session_send_error(
            session,
            FibErrorInvalidRequest,
            1U,
            frame->type,
            frame->request_id,
            "invalid cancel reason");
        return;
    }
    FibErrorCode error = 0;
    bool belongs_to_active_request = false;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    belongs_to_active_request = session->active_request && frame->request_id != 0U &&
                                frame->request_id == session->active_request_id;
    error = bridge_session_check_cancel_sequence_locked(session, frame);
    if(error == 0U) {
        session->active_request = false;
        session->state = BridgeSessionStateCancelled;
        bridge_session_set_detail_locked(session, "Request cancelled by host");
    } else if(
        error != FibErrorDuplicateSequence && error != FibErrorSequenceGap &&
        belongs_to_active_request) {
        session->active_request = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(session, "Invalid cancel message");
    }
    furi_mutex_release(session->state_mutex);
    if(error != 0U) {
        bridge_session_report_response_error(session, frame, error);
    } else {
        bridge_session_notify(session);
    }
}

static void bridge_session_handle_disconnect(BridgeSession* session, const FibFrame* frame) {
    if(frame->request_id != 0U || frame->payload_length != 1U || frame->payload[0] > 2U) {
        bridge_session_send_error(
            session, FibErrorInvalidRequest, 0U, frame->type, 0U, "invalid disconnect");
        return;
    }
    if(!bridge_session_check_control_sequence(session, frame)) return;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->helper_present = false;
    session->active_request = false;
    session->ping_pending = false;
    session->permission = BridgePermissionUnknown;
    session->selected_major = 0U;
    session->selected_minor = 0U;
    session->negotiated_capabilities = 0U;
    session->negotiated_payload = 0U;
    session->negotiated_response_bytes = 0U;
    session->state = session->usb_connected ? BridgeSessionStateWaitingForHelper :
                                              BridgeSessionStateDisconnected;
    bridge_session_set_detail_locked(session, "Desktop host closed the connection");
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
}

static bool bridge_session_is_registered_message_type(FibMessageType type) {
    switch(type) {
    case FibMessageHello:
    case FibMessageHelloAck:
    case FibMessagePermissionStatus:
    case FibMessagePermissionRequired:
    case FibMessageRequestStart:
    case FibMessageRequestHeader:
    case FibMessageRequestBodyChunk:
    case FibMessageRequestEnd:
    case FibMessageResponseStart:
    case FibMessageResponseHeader:
    case FibMessageResponseBodyChunk:
    case FibMessageResponseEnd:
    case FibMessageCancel:
    case FibMessagePing:
    case FibMessagePong:
    case FibMessageError:
    case FibMessageDisconnect:
        return true;
    default:
        return false;
    }
}

static void bridge_session_on_frame(const FibFrame* frame, void* context) {
    BridgeSession* session = context;
    bridge_session_note_activity(session);

    if(frame->major != FIB_PROTOCOL_MAJOR || frame->minor != FIB_PROTOCOL_MINOR) {
        if(frame->type == FibMessageError) {
            bridge_session_set_state(
                session, BridgeSessionStateError, "Incompatible protocol error received");
            return;
        }
        bridge_session_send_error(
            session,
            FibErrorUnsupportedVersion,
            frame->request_id ? 1U : 0U,
            frame->type,
            frame->request_id,
            "unsupported frame version");
        bridge_session_set_state(session, BridgeSessionStateError, "Incompatible protocol version");
        return;
    }

    bool handshaken;
    uint16_t negotiated_payload;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    handshaken = session->selected_major != 0U;
    negotiated_payload = session->negotiated_payload;
    furi_mutex_release(session->state_mutex);
    if(!bridge_session_is_registered_message_type(frame->type)) {
        bridge_session_send_error(
            session,
            FibErrorUnsupportedMessage,
            frame->request_id ? 1U : 0U,
            frame->type,
            frame->request_id,
            "unsupported message type");
        return;
    }
    if(!handshaken && frame->type != FibMessageHelloAck && frame->type != FibMessageError &&
       frame->type != FibMessageDisconnect) {
        bridge_session_send_error(
            session,
            FibErrorInvalidState,
            frame->request_id ? 1U : 0U,
            frame->type,
            frame->request_id,
            "handshake required");
        return;
    }
    if(handshaken && frame->type != FibMessageHelloAck &&
       frame->payload_length > negotiated_payload) {
        if(frame->type == FibMessageError) {
            bridge_session_consume_control_sequence_silently(session, frame);
            return;
        }
        bridge_session_send_error(
            session,
            FibErrorPayloadTooLarge,
            frame->request_id ? 1U : 0U,
            frame->type,
            frame->request_id,
            "negotiated payload exceeded");
        return;
    }

    switch(frame->type) {
    case FibMessageHelloAck:
        bridge_session_handle_hello_ack(session, frame);
        break;
    case FibMessagePermissionRequired:
        bridge_session_handle_permission_required(session, frame);
        break;
    case FibMessagePermissionStatus:
        bridge_session_handle_permission_status(session, frame);
        break;
    case FibMessagePing:
        bridge_session_handle_ping(session, frame);
        break;
    case FibMessagePong:
        bridge_session_handle_pong(session, frame);
        break;
    case FibMessageResponseStart:
        bridge_session_handle_response_start(session, frame);
        break;
    case FibMessageResponseHeader:
        bridge_session_handle_response_header(session, frame);
        break;
    case FibMessageResponseBodyChunk:
        bridge_session_handle_response_body(session, frame);
        break;
    case FibMessageResponseEnd:
        bridge_session_handle_response_end(session, frame);
        break;
    case FibMessageCancel:
        bridge_session_handle_cancel(session, frame);
        break;
    case FibMessageError:
        bridge_session_handle_error(session, frame);
        break;
    case FibMessageDisconnect:
        bridge_session_handle_disconnect(session, frame);
        break;
    default:
        bridge_session_send_error(
            session,
            FibErrorUnsupportedMessage,
            frame->request_id ? 1U : 0U,
            frame->type,
            frame->request_id,
            "unsupported message direction");
        break;
    }
}

static void bridge_session_on_parse_error(FibParseError error, void* context) {
    BridgeSession* session = context;
    const uint32_t now = furi_get_tick();
    FibErrorCode code = FibErrorMalformedFrame;
    if(error == FibParseErrorBadHeaderCrc) code = FibErrorBadHeaderCrc;
    if(error == FibParseErrorBadFrameCrc) code = FibErrorBadFrameCrc;
    if(error == FibParseErrorPayloadTooLarge) code = FibErrorPayloadTooLarge;
    const bool rejected_error_frame = session->parser.used > 7U &&
                                      session->parser.frame_bytes[7U] == (uint8_t)FibMessageError;

    bool send_error = false;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    bridge_session_set_detail_locked(session, "Corrupt USB frame ignored");
    if(!session->error_sent_once ||
       bridge_ticks_elapsed(now, session->last_error_sent_tick, FIB_ERROR_RATE_LIMIT_MS)) {
        session->error_sent_once = true;
        session->last_error_sent_tick = now;
        send_error = true;
    }
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
    if(send_error && !rejected_error_frame) {
        bridge_session_send_error(session, code, 0U, 0, 0U, "malformed frame");
    }
}

static void bridge_session_reset_parser(BridgeSession* session) {
    furi_mutex_acquire(session->parser_mutex, FuriWaitForever);
    fib_parser_reset(&session->parser);
    session->parser_last_progress_tick = furi_get_tick();
    furi_mutex_release(session->parser_mutex);
}

static void
    bridge_session_on_transport_receive(const uint8_t* data, size_t length, void* context) {
    BridgeSession* session = context;
    furi_mutex_acquire(session->parser_mutex, FuriWaitForever);
    session->parser_last_progress_tick = furi_get_tick();
    fib_parser_feed(&session->parser, data, length);
    furi_mutex_release(session->parser_mutex);
}

static void bridge_session_begin_handshake(BridgeSession* session) {
    uint64_t nonce = 0U;
    furi_hal_random_fill_buf((uint8_t*)&nonce, sizeof(nonce));
    if(nonce == 0U) nonce = ((uint64_t)furi_hal_random_get() << 32U) | furi_get_tick();

    bridge_session_reset_parser(session);
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->helper_present = true;
    session->permission = BridgePermissionUnknown;
    session->selected_major = 0U;
    session->selected_minor = 0U;
    session->negotiated_capabilities = 0U;
    session->negotiated_payload = 0U;
    session->negotiated_response_bytes = 0U;
    session->client_nonce = nonce;
    session->server_nonce = 0U;
    session->next_control_sequence = 1U;
    session->expected_control_sequence = 1U;
    session->active_request = false;
    session->ping_pending = false;
    session->response_started = false;
    session->state = BridgeSessionStateWaitingForHelloAck;
    session->last_activity_tick = furi_get_tick();
    bridge_session_set_detail_locked(session, "Connecting to desktop host");
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);

    if(!bridge_session_send_hello(session)) {
        bridge_session_set_state(session, BridgeSessionStateError, "Could not send HELLO");
    }
}

static void
    bridge_session_transport_lost(BridgeSession* session, bool usb_connected, const char* detail) {
    bridge_session_reset_parser(session);
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->usb_connected = usb_connected;
    session->helper_present = false;
    session->permission = BridgePermissionUnknown;
    session->selected_major = 0U;
    session->selected_minor = 0U;
    session->negotiated_capabilities = 0U;
    session->negotiated_payload = 0U;
    session->negotiated_response_bytes = 0U;
    session->active_request = false;
    session->ping_pending = false;
    session->response_started = false;
    session->state = usb_connected ? BridgeSessionStateWaitingForHelper :
                                     BridgeSessionStateDisconnected;
    bridge_session_set_detail_locked(session, detail);
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
}

static void bridge_session_on_transport_event(UsbTransportEvent event, void* context) {
    BridgeSession* session = context;
    switch(event) {
    case UsbTransportEventUsbConnected:
        furi_mutex_acquire(session->state_mutex, FuriWaitForever);
        session->usb_connected = true;
        if(!session->helper_present) {
            session->state = BridgeSessionStateWaitingForHelper;
            bridge_session_set_detail_locked(session, "Waiting for desktop host");
        }
        furi_mutex_release(session->state_mutex);
        bridge_session_notify(session);
        break;
    case UsbTransportEventUsbDisconnected:
        bridge_session_transport_lost(session, false, "USB disconnected");
        break;
    case UsbTransportEventPortOpened:
        bridge_session_begin_handshake(session);
        break;
    case UsbTransportEventPortClosed: {
        const bool connected = usb_transport_is_usb_connected(session->transport);
        bridge_session_transport_lost(
            session,
            connected,
            connected ? "Desktop host not found" : "USB disconnected");
    } break;
    case UsbTransportEventRxOverflow:
        bridge_session_reset_parser(session);
        furi_mutex_acquire(session->state_mutex, FuriWaitForever);
        session->active_request = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(session, "USB receive buffer overflow");
        furi_mutex_release(session->state_mutex);
        bridge_session_notify(session);
        bridge_session_send_error(
            session, FibErrorRxOverflow, 0U, 0, 0U, "receive buffer overflow");
        break;
    }
}

static void bridge_session_init_identity(BridgeSession* session) {
    const char* name = furi_hal_version_get_name_ptr();
    if(!name || name[0] == '\0') name = "Unknown";
    snprintf(session->device_name, sizeof(session->device_name), "%s", name);

    const uint8_t* uid = furi_hal_version_uid();
    const size_t uid_length = furi_hal_version_uid_size();
    session->uid_suffix[0] = '\0';
    if(uid && uid_length != 0U) {
        const size_t start = (uid_length > 4U) ? uid_length - 4U : 0U;
        size_t used = 0U;
        for(size_t index = start; index < uid_length && used + 2U < sizeof(session->uid_suffix);
            ++index) {
            used += (size_t)snprintf(
                session->uid_suffix + used, sizeof(session->uid_suffix) - used, "%02X", uid[index]);
        }
    }
}

BridgeSession* bridge_session_alloc_with_version(
    BridgeSessionUpdateCallback update_callback,
    void* update_context,
    const char* app_version) {
    BridgeSession* session = malloc(sizeof(BridgeSession));
    if(!session) return NULL;
    memset(session, 0, sizeof(*session));

    session->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    session->parser_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    session->tx_frame_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    session->update_callback = update_callback;
    session->update_context = update_context;
    session->state = BridgeSessionStateDisconnected;
    session->permission = BridgePermissionUnknown;
    session->declared_response_bytes = FIB_UNKNOWN_BODY_LENGTH;
    snprintf(
        session->app_version,
        sizeof(session->app_version),
        "%s",
        (app_version && app_version[0] != '\0') ? app_version : FIB_APP_VERSION);
    session->next_request_id = furi_hal_random_get();
    if(session->next_request_id == 0U) session->next_request_id = 1U;
    bridge_session_set_detail_locked(session, "Waiting for USB connection");
    bridge_session_init_identity(session);
    fib_parser_init(
        &session->parser, bridge_session_on_frame, bridge_session_on_parse_error, session);
    session->transport = usb_transport_alloc(
        bridge_session_on_transport_receive, bridge_session_on_transport_event, session);

    if(!session->state_mutex || !session->parser_mutex || !session->tx_frame_mutex ||
       !session->transport) {
        bridge_session_free(session);
        return NULL;
    }
    return session;
}

BridgeSession*
    bridge_session_alloc(BridgeSessionUpdateCallback update_callback, void* update_context) {
    return bridge_session_alloc_with_version(update_callback, update_context, FIB_APP_VERSION);
}

bool bridge_session_start(BridgeSession* session) {
    if(!session) return false;
    if(!usb_transport_start(session->transport)) {
        bridge_session_set_state(
            session, BridgeSessionStateError, "USB CDC could not start or is locked");
        return false;
    }

    const bool connected = usb_transport_is_usb_connected(session->transport);
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->usb_connected = connected;
    session->state = connected ? BridgeSessionStateWaitingForHelper :
                                 BridgeSessionStateDisconnected;
    bridge_session_set_detail_locked(
        session, connected ? "Waiting for desktop host" : "Waiting for USB connection");
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
    return true;
}

bool bridge_session_ping(BridgeSession* session) {
    if(!session) return false;
    uint64_t token = 0U;
    furi_hal_random_fill_buf((uint8_t*)&token, sizeof(token));
    if(token == 0U) token = furi_get_tick();

    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    const bool can_ping = session->helper_present && session->selected_major != 0U &&
                          bridge_session_permission_is_allowed(session->permission) &&
                          !session->ping_pending && !session->active_request;
    if(can_ping) {
        session->ping_pending = true;
        session->ping_token = token;
        session->ping_started_tick = furi_get_tick();
        session->state = BridgeSessionStatePinging;
        bridge_session_set_detail_locked(session, "Testing connection");
    } else {
        if(!session->usb_connected) {
            bridge_session_set_detail_locked(session, "No USB connection");
        } else if(!session->helper_present) {
            bridge_session_set_detail_locked(session, "Desktop host not found");
        } else if(!bridge_session_permission_is_allowed(session->permission)) {
            bridge_session_set_detail_locked(session, "Internet permission not granted");
        } else {
            bridge_session_set_detail_locked(session, "Bridge is not ready yet");
        }
    }
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
    if(!can_ping) return false;

    uint8_t payload[8];
    fib_write_u64_le(payload, token);
    if(!bridge_session_send_frame(
           session,
           FibMessagePing,
           0U,
           0U,
           bridge_session_take_control_sequence(session),
           payload,
           sizeof(payload))) {
        furi_mutex_acquire(session->state_mutex, FuriWaitForever);
        session->ping_pending = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(session, "Could not send PING");
        furi_mutex_release(session->state_mutex);
        bridge_session_notify(session);
        return false;
    }
    return true;
}

static bool bridge_session_valid_https_url(const char* url, size_t* length) {
    if(!url) return false;
    const size_t url_length = strlen(url);
    if(length) *length = url_length;
    if(url_length < 9U || url_length > FIB_MAX_URL_LENGTH || strncmp(url, "https://", 8U) != 0) {
        return false;
    }
    for(size_t index = 0U; index < url_length; ++index) {
        const uint8_t value = (uint8_t)url[index];
        if(value < 0x20U || value == 0x7FU) return false;
    }
    return true;
}

static bool bridge_session_request_get_internal(
    BridgeSession* session,
    const char* url,
    uint32_t timeout_ms,
    bool radio_mode) {
    if(!session) return false;
    size_t url_length = 0U;
    if(!bridge_session_valid_https_url(url, &url_length)) {
        bridge_session_set_state(
            session, BridgeSessionStateError, "Only valid HTTPS URLs are accepted");
        return false;
    }
    if(timeout_ms == 0U) timeout_ms = FIB_DEFAULT_REQUEST_TIMEOUT_MS;
    if(timeout_ms > FIB_MAX_REQUEST_TIMEOUT_MS) timeout_ms = FIB_MAX_REQUEST_TIMEOUT_MS;

    uint32_t request_id = 0U;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    const bool payload_fits = 12U + url_length <= session->negotiated_payload;
    const bool headers_supported =
        !radio_mode ||
        (session->negotiated_capabilities & FIB_CAPABILITY_REQUEST_HEADERS) != 0U;
    const bool request_id_available = session->next_request_id != 0U;
    const bool ready = bridge_session_permission_is_allowed(session->permission) &&
                       session->helper_present && session->selected_major != 0U && payload_fits &&
                       headers_supported && request_id_available && !session->active_request &&
                       !session->ping_pending;
    if(ready) {
        request_id = session->next_request_id;
        /* FIBP v1 forbids request-ID wrap within one app session. Zero marks
         * exhaustion so a UINT32_MAX request can finish but the next request
         * asks the user to restart the FAP and create a fresh session. */
        session->next_request_id = (request_id == UINT32_MAX) ? 0U : (request_id + 1U);
        session->active_request = true;
        session->active_request_id = request_id;
        session->request_timeout_ms = timeout_ms;
        session->request_started_tick = furi_get_tick();
        session->next_request_sequence = 0U;
        session->response_started = false;
        session->response_body_started = false;
        session->expected_response_sequence = 0U;
        session->response_bytes = 0U;
        session->declared_response_bytes = FIB_UNKNOWN_BODY_LENGTH;
        session->response_truncated = false;
        session->preview_length = 0U;
        session->preview[0] = '\0';
        session->http_status = 0U;
        session->state = BridgeSessionStateSendingRequest;
        bridge_session_set_detail_locked(session, "Sending HTTPS request");
    } else {
        if(!session->usb_connected) {
            bridge_session_set_detail_locked(session, "No USB connection");
        } else if(!session->helper_present) {
            bridge_session_set_detail_locked(session, "Desktop host not found");
        } else if(session->selected_major == 0U) {
            bridge_session_set_detail_locked(session, "Waiting for protocol handshake");
        } else if(!bridge_session_permission_is_allowed(session->permission)) {
            bridge_session_set_detail_locked(session, "Internet permission not granted");
        } else if(!payload_fits) {
            bridge_session_set_detail_locked(session, "URL exceeds the packet size limit");
        } else if(!headers_supported) {
            bridge_session_set_detail_locked(session, "Desktop host is too old for radio streaming");
        } else if(!request_id_available) {
            bridge_session_set_detail_locked(
                session, "Request IDs exhausted; restart the app");
        } else {
            bridge_session_set_detail_locked(session, "Another request is active");
        }
    }
    furi_mutex_release(session->state_mutex);
    bridge_session_notify(session);
    if(!ready) return false;

    uint8_t payload[12U + FIB_MAX_URL_LENGTH];
    size_t used = 0U;
    payload[used++] = 1U;
    fib_write_u32_le(payload + used, timeout_ms);
    used += 4U;
    fib_write_u16_le(payload + used, (uint16_t)url_length);
    used += 2U;
    fib_write_u32_le(payload + used, 0U);
    used += 4U;
    payload[used++] = radio_mode ? 1U : 0U;
    memcpy(payload + used, url, url_length);
    used += url_length;

    bool sent = bridge_session_send_frame(
        session, FibMessageRequestStart, 0U, request_id, 0U, payload, used);
    if(sent && radio_mode) {
        static const char header_name[] = "accept";
        static const char header_value[] = "audio/mpeg";
        uint8_t header_payload[3U + sizeof(header_name) - 1U + sizeof(header_value) - 1U];
        size_t header_used = 0U;
        header_payload[header_used++] = sizeof(header_name) - 1U;
        fib_write_u16_le(header_payload + header_used, sizeof(header_value) - 1U);
        header_used += 2U;
        memcpy(header_payload + header_used, header_name, sizeof(header_name) - 1U);
        header_used += sizeof(header_name) - 1U;
        memcpy(header_payload + header_used, header_value, sizeof(header_value) - 1U);
        header_used += sizeof(header_value) - 1U;
        sent = bridge_session_send_frame(
            session, FibMessageRequestHeader, 0U, request_id, 1U, header_payload, header_used);
    }
    if(sent) {
        sent = bridge_session_send_frame(
            session,
            FibMessageRequestEnd,
            FibFlagFinal,
            request_id,
            radio_mode ? 2U : 1U,
            NULL,
            0U);
    }
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->next_request_sequence = radio_mode ? 3U : 2U;
    if(!sent) {
        session->active_request = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(session, "Could not send request over USB");
    }
    furi_mutex_release(session->state_mutex);
    if(!sent) bridge_session_notify(session);
    return sent;
}

bool bridge_session_request_get(BridgeSession* session, const char* url, uint32_t timeout_ms) {
    return bridge_session_request_get_internal(session, url, timeout_ms, false);
}

bool bridge_session_request_radio(BridgeSession* session, const char* url, uint32_t timeout_ms) {
    return bridge_session_request_get_internal(session, url, timeout_ms, true);
}

bool bridge_session_cancel(BridgeSession* session) {
    if(!session) return false;
    uint32_t request_id;
    uint32_t sequence;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    const bool active = session->active_request;
    const bool can_send = active && bridge_session_can_cancel_locked(session);
    request_id = session->active_request_id;
    sequence = can_send ? session->next_request_sequence++ : 0U;
    if(active) {
        session->active_request = false;
        session->state = BridgeSessionStateCancelled;
        bridge_session_set_detail_locked(session, "Request cancelled");
    }
    furi_mutex_release(session->state_mutex);
    if(!active) return false;
    const bool sent = can_send && bridge_session_send_cancel(session, request_id, sequence, 0U);
    bridge_session_notify(session);
    return sent;
}

void bridge_session_set_body_callback(
    BridgeSession* session, BridgeSessionBodyCallback callback, void* context) {
    if(!session) return;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    session->body_callback = callback;
    session->body_context = context;
    furi_mutex_release(session->state_mutex);
}

void bridge_session_tick(BridgeSession* session) {
    if(!session) return;
    const uint32_t now = furi_get_tick();
    bool resend_hello = false;
    bool request_timeout = false;
    bool request_timeout_cancel = false;
    bool ping_timeout = false;
    bool idle_ping = false;
    uint32_t request_id = 0U;
    uint32_t request_sequence = 0U;

    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    if(session->helper_present &&
       (session->state == BridgeSessionStateWaitingForHelloAck ||
        session->state == BridgeSessionStateHelperNotFound) &&
       bridge_ticks_elapsed(now, session->hello_sent_tick, FIB_HANDSHAKE_TIMEOUT_MS)) {
        session->state = BridgeSessionStateHelperNotFound;
        bridge_session_set_detail_locked(session, "Helper application not found");
        resend_hello = true;
    }
    if(session->active_request &&
       bridge_ticks_elapsed(now, session->request_started_tick, session->request_timeout_ms)) {
        request_timeout = true;
        request_timeout_cancel =
            bridge_session_detach_active_request_locked(session, &request_id, &request_sequence);
        session->state = BridgeSessionStateTimedOut;
        bridge_session_set_detail_locked(session, "Request timed out");
    }
    if(session->ping_pending &&
       bridge_ticks_elapsed(now, session->ping_started_tick, FIB_PONG_TIMEOUT_MS)) {
        session->ping_pending = false;
        session->state = BridgeSessionStateTimedOut;
        bridge_session_set_detail_locked(session, "Connection test timed out");
        ping_timeout = true;
    }
    if(!session->active_request && !session->ping_pending &&
       session->state == BridgeSessionStateReady &&
       bridge_ticks_elapsed(now, session->last_activity_tick, FIB_IDLE_TIMEOUT_MS)) {
        idle_ping = true;
    }
    furi_mutex_release(session->state_mutex);

    bool assembly_timeout = false;
    bool assembly_was_error = false;
    furi_mutex_acquire(session->parser_mutex, FuriWaitForever);
    if(session->parser.reading_frame &&
       bridge_ticks_elapsed(
           now, session->parser_last_progress_tick, FIB_FRAME_ASSEMBLY_TIMEOUT_MS)) {
        assembly_was_error = session->parser.used > 7U &&
                             session->parser.frame_bytes[7U] == (uint8_t)FibMessageError;
        fib_parser_reset(&session->parser);
        session->parser_last_progress_tick = now;
        assembly_timeout = true;
    }
    furi_mutex_release(session->parser_mutex);

    if(resend_hello) bridge_session_send_hello(session);
    if(request_timeout_cancel) {
        bridge_session_send_cancel(session, request_id, request_sequence, 1U);
    }
    if(assembly_timeout) {
        uint32_t assembly_request_id = 0U;
        uint32_t assembly_request_sequence = 0U;
        furi_mutex_acquire(session->state_mutex, FuriWaitForever);
        const bool cancel_request = bridge_session_detach_active_request_locked(
            session, &assembly_request_id, &assembly_request_sequence);
        session->ping_pending = false;
        session->state = BridgeSessionStateError;
        bridge_session_set_detail_locked(session, "Incomplete USB frame timed out");
        furi_mutex_release(session->state_mutex);
        if(cancel_request) {
            bridge_session_send_cancel(
                session, assembly_request_id, assembly_request_sequence, 1U);
        }
        if(!assembly_was_error) {
            bridge_session_send_error(
                session, FibErrorMalformedFrame, 0U, 0, 0U, "frame assembly timeout");
        }
        bridge_session_notify(session);
    } else if(resend_hello || request_timeout || ping_timeout) {
        bridge_session_notify(session);
    }
    if(idle_ping) bridge_session_ping(session);
}

bool bridge_session_has_active_request(BridgeSession* session) {
    if(!session) return false;
    bool active;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    active = session->active_request;
    furi_mutex_release(session->state_mutex);
    return active;
}

void bridge_session_get_snapshot(BridgeSession* session, BridgeSessionSnapshot* snapshot) {
    if(!session || !snapshot) return;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->state = session->state;
    snapshot->permission = session->permission;
    snapshot->usb_connected = session->usb_connected;
    snapshot->helper_present = session->helper_present;
    snapshot->active_request = session->active_request;
    snapshot->response_truncated = session->response_truncated;
    snapshot->selected_major = session->selected_major;
    snapshot->selected_minor = session->selected_minor;
    snapshot->http_status = session->http_status;
    snapshot->active_request_id = session->active_request_id;
    snapshot->response_bytes = session->response_bytes;
    snapshot->declared_response_bytes = session->declared_response_bytes;
    memcpy(snapshot->device_name, session->device_name, sizeof(snapshot->device_name));
    memcpy(snapshot->uid_suffix, session->uid_suffix, sizeof(snapshot->uid_suffix));
    memcpy(snapshot->detail, session->detail, sizeof(snapshot->detail));
    memcpy(snapshot->preview, session->preview, sizeof(snapshot->preview));
    furi_mutex_release(session->state_mutex);
}

void bridge_session_get_status(BridgeSession* session, BridgeSessionStatus* status) {
    if(!session || !status) return;
    furi_mutex_acquire(session->state_mutex, FuriWaitForever);
    memset(status, 0, sizeof(*status));
    status->state = session->state;
    status->permission = session->permission;
    status->usb_connected = session->usb_connected;
    status->helper_present = session->helper_present;
    status->active_request = session->active_request;
    status->response_truncated = session->response_truncated;
    status->selected_major = session->selected_major;
    status->selected_minor = session->selected_minor;
    status->http_status = session->http_status;
    status->active_request_id = session->active_request_id;
    status->response_bytes = session->response_bytes;
    status->declared_response_bytes = session->declared_response_bytes;
    memcpy(status->detail, session->detail, sizeof(status->detail));
    furi_mutex_release(session->state_mutex);
}

void bridge_session_free(BridgeSession* session) {
    if(!session) return;

    if(session->state_mutex) {
        furi_mutex_acquire(session->state_mutex, FuriWaitForever);
        session->closing = true;
        session->update_callback = NULL;
        session->update_context = NULL;
        furi_mutex_release(session->state_mutex);
    }

    if(session->transport && usb_transport_is_port_open(session->transport)) {
        const uint8_t reason = 0U;
        bridge_session_send_frame(
            session,
            FibMessageDisconnect,
            FibFlagFinal,
            0U,
            bridge_session_take_control_sequence(session),
            &reason,
            1U);
    }
    if(session->transport) usb_transport_free(session->transport);
    if(session->tx_frame_mutex) furi_mutex_free(session->tx_frame_mutex);
    if(session->parser_mutex) furi_mutex_free(session->parser_mutex);
    if(session->state_mutex) furi_mutex_free(session->state_mutex);
    free(session);
}
