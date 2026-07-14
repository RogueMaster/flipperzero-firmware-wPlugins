#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include <furi_hal.h>
#include "bad_usb_hid.h"

typedef enum {
    ClippyBadUsbStateInit,
    ClippyBadUsbStateNotConnected,
    ClippyBadUsbStateIdle,
    ClippyBadUsbStateWillRun,
    ClippyBadUsbStateRunning,
    ClippyBadUsbStateWaitForBtn,
    ClippyBadUsbStateDone,
    ClippyBadUsbStateInitError,
} ClippyBadUsbWorkerState;

typedef struct {
    ClippyBadUsbWorkerState state;
    size_t string_length;
    size_t string_idx;
    char error[64];
} ClippyBadUsbState;

typedef struct BadUsbPayload BadUsbPayload;

BadUsbPayload*
    clippy_bad_usb_payload_setup(FuriString* payload, BadUsbHidInterface interface, uint32_t delay);

void clippy_bad_usb_payload_teardown(BadUsbPayload* bad_usb);

void clippy_bad_usb_payload_set_keyboard_layout(BadUsbPayload* bad_usb, FuriString* layout_path);

void clippy_bad_usb_script_start(BadUsbPayload* bad_usb);

void clippy_bad_usb_script_stop(BadUsbPayload* bad_usb);

void clippy_bad_usb_payload_start_stop(BadUsbPayload* bad_usb);

void clippy_bad_usb_payload_pause_resume(BadUsbPayload* bad_usb);

ClippyBadUsbState* clippy_bad_usb_payload_get_state(BadUsbPayload* bad_usb);

#ifdef __cplusplus
}
#endif
