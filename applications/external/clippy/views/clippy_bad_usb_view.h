#pragma once

#include <gui/view.h>
#include "../helpers/ducky_script.h"

typedef struct ClippyBadUsb ClippyBadUsb;
typedef void (*ClippyBadUsbButtonCallback)(InputKey key, void* context);

ClippyBadUsb* clippy_bad_usb_view_alloc(void);

void clippy_bad_usb_view_free(ClippyBadUsb* bad_usb);

View* clippy_bad_usb_view_get_view(ClippyBadUsb* bad_usb);

void clippy_bad_usb_view_set_button_callback(
    ClippyBadUsb* bad_usb,
    ClippyBadUsbButtonCallback callback,
    void* context);

void clippy_bad_usb_view_set_layout(ClippyBadUsb* bad_usb, const char* layout);

void clippy_bad_usb_view_set_state(ClippyBadUsb* bad_usb, ClippyBadUsbState* st);

bool clippy_bad_usb_view_is_idle_state(ClippyBadUsb* bad_usb);
