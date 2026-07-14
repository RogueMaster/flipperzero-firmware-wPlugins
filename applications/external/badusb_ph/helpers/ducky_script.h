#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include <furi_hal.h>
#include "bad_usb_hid.h"
#include "placeholder.h"

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

void bad_usb_script_set_keyboard_layout(BadUsbScript* bad_usb, FuriString* layout_path);

// Sets the placeholder map applied to every line before it is interpreted.
// Pass NULL to disable substitution. The map must outlive the script.
void bad_usb_script_set_placeholders(BadUsbScript* bad_usb, const PlaceholderMap* map);

void bad_usb_script_start(BadUsbScript* bad_usb);

void bad_usb_script_stop(BadUsbScript* bad_usb);

void bad_usb_script_start_stop(BadUsbScript* bad_usb);

void bad_usb_script_pause_resume(BadUsbScript* bad_usb);

BadUsbState* bad_usb_script_get_state(BadUsbScript* bad_usb);

#ifdef __cplusplus
}
#endif
