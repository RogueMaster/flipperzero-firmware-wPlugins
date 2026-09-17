#include "fib_bridge_client.h"

#include "../../bridge_session.h"

#include <stdlib.h>
#include <string.h>

struct FibBridgeClient {
    BridgeSession* session;
    FibBridgeClientCallbacks callbacks;
};

static FibBridgeState fib_bridge_map_state(BridgeSessionState state) {
    switch(state) {
    case BridgeSessionStateDisconnected:
        return FibBridgeStateDisconnected;
    case BridgeSessionStateWaitingForHelper:
        return FibBridgeStateWaitingForHost;
    case BridgeSessionStateWaitingForHelloAck:
        return FibBridgeStateHandshaking;
    case BridgeSessionStateHelperNotFound:
        return FibBridgeStateHostNotFound;
    case BridgeSessionStatePermissionPending:
        return FibBridgeStatePermissionPending;
    case BridgeSessionStatePermissionDenied:
        return FibBridgeStatePermissionDenied;
    case BridgeSessionStateReady:
        return FibBridgeStateReady;
    case BridgeSessionStatePinging:
        return FibBridgeStatePinging;
    case BridgeSessionStateSendingRequest:
        return FibBridgeStateSendingRequest;
    case BridgeSessionStateReceivingResponse:
        return FibBridgeStateReceivingResponse;
    case BridgeSessionStateComplete:
        return FibBridgeStateComplete;
    case BridgeSessionStateCancelled:
        return FibBridgeStateCancelled;
    case BridgeSessionStateTimedOut:
        return FibBridgeStateTimedOut;
    case BridgeSessionStateError:
    default:
        return FibBridgeStateError;
    }
}

static void
    fib_bridge_copy_status(const BridgeSessionStatus* source, FibBridgeStatus* destination) {
    memset(destination, 0, sizeof(*destination));
    destination->state = fib_bridge_map_state(source->state);
    destination->permission = (FibBridgePermission)source->permission;
    destination->usb_connected = source->usb_connected;
    destination->host_present = source->helper_present;
    destination->active_request = source->active_request;
    destination->response_truncated = source->response_truncated;
    destination->protocol_major = source->selected_major;
    destination->protocol_minor = source->selected_minor;
    destination->http_status = source->http_status;
    destination->request_id = source->active_request_id;
    destination->response_bytes = source->response_bytes;
    destination->declared_response_bytes = source->declared_response_bytes;
    memcpy(destination->detail, source->detail, sizeof(destination->detail));
    destination->detail[sizeof(destination->detail) - 1U] = '\0';
}

static void fib_bridge_on_session_update(void* context) {
    FibBridgeClient* client = context;
    if(!client || !client->callbacks.on_status) return;
    BridgeSessionStatus source;
    FibBridgeStatus status;
    bridge_session_get_status(client->session, &source);
    fib_bridge_copy_status(&source, &status);
    client->callbacks.on_status(client->callbacks.context, &status);
}

static bool fib_bridge_on_body(void* context, const uint8_t* data, size_t length) {
    FibBridgeClient* client = context;
    if(!client || !client->callbacks.on_body) return true;
    return client->callbacks.on_body(client->callbacks.context, data, length);
}

FibBridgeClient* fib_bridge_client_alloc(
    const FibBridgeClientConfig* config,
    const FibBridgeClientCallbacks* callbacks) {
    FibBridgeClient* client = malloc(sizeof(FibBridgeClient));
    if(!client) return NULL;
    memset(client, 0, sizeof(*client));
    if(callbacks) client->callbacks = *callbacks;

    const char* version = config ? config->app_version : NULL;
    client->session =
        bridge_session_alloc_with_version(fib_bridge_on_session_update, client, version);
    if(!client->session) {
        free(client);
        return NULL;
    }
    bridge_session_set_body_callback(client->session, fib_bridge_on_body, client);
    return client;
}

void fib_bridge_client_free(FibBridgeClient* client) {
    if(!client) return;
    if(client->session) {
        bridge_session_set_body_callback(client->session, NULL, NULL);
        bridge_session_free(client->session);
    }
    memset(client, 0, sizeof(*client));
    free(client);
}

bool fib_bridge_client_start(FibBridgeClient* client) {
    return client && bridge_session_start(client->session);
}

void fib_bridge_client_tick(FibBridgeClient* client) {
    if(client) bridge_session_tick(client->session);
}

bool fib_bridge_client_ping(FibBridgeClient* client) {
    return client && bridge_session_ping(client->session);
}

bool fib_bridge_client_get(FibBridgeClient* client, const char* url, uint32_t timeout_ms) {
    return client && bridge_session_request_get(client->session, url, timeout_ms);
}

bool fib_bridge_client_cancel(FibBridgeClient* client) {
    return client && bridge_session_cancel(client->session);
}

bool fib_bridge_client_is_ready(FibBridgeClient* client) {
    if(!client) return false;
    BridgeSessionStatus snapshot;
    bridge_session_get_status(client->session, &snapshot);
    return snapshot.state == BridgeSessionStateReady ||
           snapshot.state == BridgeSessionStateComplete;
}

bool fib_bridge_client_has_active_request(FibBridgeClient* client) {
    return client && bridge_session_has_active_request(client->session);
}

void fib_bridge_client_get_status(FibBridgeClient* client, FibBridgeStatus* status) {
    if(!status) return;
    memset(status, 0, sizeof(*status));
    if(!client) {
        status->state = FibBridgeStateError;
        return;
    }
    BridgeSessionStatus source;
    bridge_session_get_status(client->session, &source);
    fib_bridge_copy_status(&source, status);
}
