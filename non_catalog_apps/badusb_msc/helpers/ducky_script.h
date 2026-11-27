#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include <furi_hal.h>
#include "bad_usb_hid.h"

typedef enum {
    BadUsbStateInit,
    BadUsbStateNotConnected,
    BadUsbStateIdle,
    BadUsbStateWillRun,
    BadUsbStateRunning,
    BadUsbStateDelay,
    BadUsbStateStringDelay,
    BadUsbStateWaitForBtn,
    BadUsbStatePaused,
    BadUsbStateDone,
    BadUsbStateScriptError,
    BadUsbStateFileError,
} BadUsbWorkerState;

typedef struct {
    BadUsbWorkerState state;
    size_t line_cur;
    size_t line_nb;
    uint32_t delay_remain;
    size_t error_line;
    char error[64];
    uint32_t elapsed;
} BadUsbState;

typedef struct BadUsbScript BadUsbScript;

BadUsbScript* bad_usb_script_open(
    FuriString* file_path,
    BadUsbHidInterface* interface,
    BadUsbHidConfig* hid_cfg,
    bool load_id_cfg);

void bad_usb_script_close(BadUsbScript* bad_usb);

// Validate and read the exact mapping the script engine uses (128 x uint16_t).
// A filename in the menu is not proof the map was loaded.
bool bad_usb_keyboard_layout_read(FuriString* layout_path, uint16_t layout[128]);

// Applies first-line DuckyScript ID to the same USB identity that will
// be advertised by the composite USB device. The worker parses it again.
bool bad_usb_script_preload_usb_identity(FuriString* script_path, BadUsbHidConfig* hid_cfg);

// Returns false if a non-US layout cannot actually be loaded.
bool bad_usb_script_set_keyboard_layout(BadUsbScript* bad_usb, FuriString* layout_path);

void bad_usb_script_start(BadUsbScript* bad_usb);

void bad_usb_script_stop(BadUsbScript* bad_usb);

void bad_usb_script_start_stop(BadUsbScript* bad_usb);

void bad_usb_script_pause_resume(BadUsbScript* bad_usb);

BadUsbState* bad_usb_script_get_state(BadUsbScript* bad_usb);

#ifdef __cplusplus
}
#endif
