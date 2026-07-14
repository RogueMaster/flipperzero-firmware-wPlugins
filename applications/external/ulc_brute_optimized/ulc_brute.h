#pragma once

#include <furi_hal.h>
#include <notification/notification_messages.h>
#include <gui/gui.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>

#define TAG "ULCBrute"

typedef enum {
    AppStateWaiting,
    AppStateBruteforcing,
    AppStateBruteComplete,
    AppStateError,
} AppState;

typedef enum {
    AppModeReady,
    AppModeRunning,
} AppMode;

// Segment of the key to bruteforce
typedef enum {
    AppKeyModeKey1,
    AppKeyModeKey2,
    AppKeyModeKey3,
    AppKeyModeKey4,
} AppKeyMode;

typedef enum {
    EventTypeTick,
    EventTypeKey,
    EventTypeRx,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    // Most frequently accessed members first
    uint32_t current_key_index;
    uint8_t key[16] __attribute__((aligned(4)));
    uint8_t auth2_data[16] __attribute__((aligned(4)));
    uint8_t shared_buffer[32] __attribute__((aligned(4)));
    mbedtls_des3_context ctx;
    uint32_t time_start;
    FuriThread* bruteforce_thread;
    AppState state;
    AppMode mode;
    AppKeyMode key_mode;
    const char* error;
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* nfc_mutex;
    NotificationApp* notifications;
    FuriMessageQueue* event_queue;
    bool bruteforce_running;
    bool field_active;
} AppContext;
