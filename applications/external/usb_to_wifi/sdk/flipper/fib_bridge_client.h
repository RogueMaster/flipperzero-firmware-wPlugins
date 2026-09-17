#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FibBridgeClient FibBridgeClient;

typedef enum {
    FibBridgeStateDisconnected,
    FibBridgeStateWaitingForHost,
    FibBridgeStateHandshaking,
    FibBridgeStateHostNotFound,
    FibBridgeStatePermissionPending,
    FibBridgeStatePermissionDenied,
    FibBridgeStateReady,
    FibBridgeStatePinging,
    FibBridgeStateSendingRequest,
    FibBridgeStateReceivingResponse,
    FibBridgeStateComplete,
    FibBridgeStateCancelled,
    FibBridgeStateTimedOut,
    FibBridgeStateError,
} FibBridgeState;

typedef enum {
    FibBridgePermissionUnknown,
    FibBridgePermissionPending,
    FibBridgePermissionDenied,
    FibBridgePermissionAllowedOnce,
    FibBridgePermissionAllowedAlways,
} FibBridgePermission;

typedef struct {
    FibBridgeState state;
    FibBridgePermission permission;
    bool usb_connected;
    bool host_present;
    bool active_request;
    bool response_truncated;
    uint8_t protocol_major;
    uint8_t protocol_minor;
    uint16_t http_status;
    uint32_t request_id;
    uint32_t response_bytes;
    uint32_t declared_response_bytes;
    char detail[129];
} FibBridgeStatus;

typedef struct {
    /* Sent in HELLO. UTF-8, 1...16 bytes. Defaults to the bridge version. */
    const char* app_version;
} FibBridgeClientConfig;

/* Callbacks run on the bridge transport worker. Keep them short and never call
 * fib_bridge_client_free() from inside a callback. Returning false from
 * on_body aborts the active response. */
typedef void (*FibBridgeStatusCallback)(void* context, const FibBridgeStatus* status);
typedef bool (*FibBridgeBodyCallback)(void* context, const uint8_t* data, size_t length);

typedef struct {
    FibBridgeStatusCallback on_status;
    FibBridgeBodyCallback on_body;
    void* context;
} FibBridgeClientCallbacks;

FibBridgeClient* fib_bridge_client_alloc(
    const FibBridgeClientConfig* config,
    const FibBridgeClientCallbacks* callbacks);
void fib_bridge_client_free(FibBridgeClient* client);

bool fib_bridge_client_start(FibBridgeClient* client);
void fib_bridge_client_tick(FibBridgeClient* client);
bool fib_bridge_client_ping(FibBridgeClient* client);

/* FIBP v1 supports one active HTTPS GET at a time. A zero timeout selects the
 * default; larger values are clamped to the protocol maximum. */
bool fib_bridge_client_get(FibBridgeClient* client, const char* url, uint32_t timeout_ms);
bool fib_bridge_client_cancel(FibBridgeClient* client);
bool fib_bridge_client_is_ready(FibBridgeClient* client);
bool fib_bridge_client_has_active_request(FibBridgeClient* client);
void fib_bridge_client_get_status(FibBridgeClient* client, FibBridgeStatus* status);

#ifdef __cplusplus
}
#endif
