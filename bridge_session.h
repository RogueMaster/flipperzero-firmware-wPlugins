#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BridgeSession BridgeSession;

typedef enum {
    BridgeSessionStateDisconnected,
    BridgeSessionStateWaitingForHelper,
    BridgeSessionStateWaitingForHelloAck,
    BridgeSessionStateHelperNotFound,
    BridgeSessionStatePermissionPending,
    BridgeSessionStatePermissionDenied,
    BridgeSessionStateReady,
    BridgeSessionStatePinging,
    BridgeSessionStateSendingRequest,
    BridgeSessionStateReceivingResponse,
    BridgeSessionStateComplete,
    BridgeSessionStateCancelled,
    BridgeSessionStateTimedOut,
    BridgeSessionStateError,
} BridgeSessionState;

typedef enum {
    BridgePermissionUnknown,
    BridgePermissionPending,
    BridgePermissionDenied,
    BridgePermissionAllowedOnce,
    BridgePermissionAllowedAlways,
} BridgePermission;

typedef struct {
    BridgeSessionState state;
    BridgePermission permission;
    bool usb_connected;
    bool helper_present;
    bool active_request;
    bool response_truncated;
    uint8_t selected_major;
    uint8_t selected_minor;
    uint16_t http_status;
    uint32_t active_request_id;
    uint32_t response_bytes;
    uint32_t declared_response_bytes;
    char device_name[33];
    char uid_suffix[9];
    char detail[FIB_MAX_ERROR_DETAIL + 1U];
    char preview[FIB_RESPONSE_PREVIEW_SIZE + 1U];
} BridgeSessionSnapshot;

typedef struct {
    BridgeSessionState state;
    BridgePermission permission;
    bool usb_connected;
    bool helper_present;
    bool active_request;
    bool response_truncated;
    uint8_t selected_major;
    uint8_t selected_minor;
    uint16_t http_status;
    uint32_t active_request_id;
    uint32_t response_bytes;
    uint32_t declared_response_bytes;
    char detail[FIB_MAX_ERROR_DETAIL + 1U];
} BridgeSessionStatus;

typedef void (*BridgeSessionUpdateCallback)(void* context);
typedef bool (*BridgeSessionBodyCallback)(
    void* context, const uint8_t* data, size_t length);

BridgeSession*
    bridge_session_alloc(BridgeSessionUpdateCallback update_callback, void* update_context);
BridgeSession* bridge_session_alloc_with_version(
    BridgeSessionUpdateCallback update_callback,
    void* update_context,
    const char* app_version);
void bridge_session_free(BridgeSession* session);

bool bridge_session_start(BridgeSession* session);
void bridge_session_tick(BridgeSession* session);

bool bridge_session_ping(BridgeSession* session);
bool bridge_session_request_get(BridgeSession* session, const char* url, uint32_t timeout_ms);
bool bridge_session_request_radio(BridgeSession* session, const char* url, uint32_t timeout_ms);
void bridge_session_set_body_callback(
    BridgeSession* session, BridgeSessionBodyCallback callback, void* context);
bool bridge_session_cancel(BridgeSession* session);

bool bridge_session_has_active_request(BridgeSession* session);
void bridge_session_get_snapshot(BridgeSession* session, BridgeSessionSnapshot* snapshot);
void bridge_session_get_status(BridgeSession* session, BridgeSessionStatus* status);

#ifdef __cplusplus
}
#endif
