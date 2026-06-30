#pragma once

#include <gui/view.h>
#include "../helpers/flipper_wedge_custom_event.h"

// Forward declaration
typedef struct FlipperWedgeStartscreen FlipperWedgeStartscreen;

// Scan states for display
typedef enum {
    FlipperWedgeDisplayStateIdle,
    FlipperWedgeDisplayStateScanning,
    FlipperWedgeDisplayStateWaiting,
    FlipperWedgeDisplayStateResult,
    FlipperWedgeDisplayStateSent,
} FlipperWedgeDisplayState;

typedef void (*FlipperWedgeStartscreenCallback)(FlipperWedgeCustomEvent event, void* context);

void flipper_wedge_startscreen_set_callback(
    FlipperWedgeStartscreen* flipper_wedge_startscreen,
    FlipperWedgeStartscreenCallback callback,
    void* context);

View* flipper_wedge_startscreen_get_view(FlipperWedgeStartscreen* flipper_wedge_static);

FlipperWedgeStartscreen* flipper_wedge_startscreen_alloc();

void flipper_wedge_startscreen_free(FlipperWedgeStartscreen* flipper_wedge_static);

void flipper_wedge_startscreen_set_connected_status(
    FlipperWedgeStartscreen* instance,
    bool usb_connected,
    bool bt_connected);

void flipper_wedge_startscreen_set_mode(
    FlipperWedgeStartscreen* instance,
    uint8_t mode);

uint8_t flipper_wedge_startscreen_get_mode(FlipperWedgeStartscreen* instance);

void flipper_wedge_startscreen_set_display_state(
    FlipperWedgeStartscreen* instance,
    FlipperWedgeDisplayState state);

void flipper_wedge_startscreen_set_status_text(
    FlipperWedgeStartscreen* instance,
    const char* text);

void flipper_wedge_startscreen_set_uid_text(
    FlipperWedgeStartscreen* instance,
    const char* text);
