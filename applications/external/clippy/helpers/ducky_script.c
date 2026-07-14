#include "core/kernel.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <lib/toolbox/args.h>
#include <lib/toolbox/strint.h>
#include <storage/storage.h>
#include "ducky_script.h"
#include "ducky_script_i.h"
#include <dolphin/dolphin.h>

#define TAG "BadUsb"

#define WORKER_TAG TAG "Worker"

#define BADUSB_ASCII_TO_KEY(script, x) \
    (((uint8_t)x < 128) ? (script->layout[(uint8_t)x]) : HID_KEYBOARD_NONE)

typedef enum {
    WorkerEvtStartStop = (1 << 0),
    WorkerEvtEnd = (1 << 1),
    WorkerEvtConnect = (1 << 2),
    WorkerEvtDisconnect = (1 << 3),
} WorkerEvtFlags;

uint16_t ducky_get_keycode(BadUsbPayload* bad_usb, const char* param, bool accept_chars) {
    uint16_t keycode = ducky_get_keycode_by_name(param);
    if(keycode != HID_KEYBOARD_NONE) {
        return keycode;
    }

    if((accept_chars) && (strlen(param) > 0)) {
        return BADUSB_ASCII_TO_KEY(bad_usb, param[0]) & 0xFF;
    }
    return 0;
}

bool ducky_get_number(const char* param, uint32_t* val) {
    uint32_t value = 0;
    if(strint_to_uint32(param, NULL, &value, 10) == StrintParseNoError) {
        *val = value;
        return true;
    }
    return false;
}

int32_t ducky_error(BadUsbPayload* bad_usb, const char* text, ...) {
    va_list args;
    va_start(args, text);

    vsnprintf(bad_usb->st.error, sizeof(bad_usb->st.error), text, args);

    va_end(args);
    return SCRIPT_STATE_ERROR;
}

static bool ducky_string_next(BadUsbPayload* bad_usb) {
    if(bad_usb->string_print_pos >= furi_string_size(bad_usb->string_print)) {
        return true;
    }

    char print_char = furi_string_get_char(bad_usb->string_print, bad_usb->string_print_pos);

    if(print_char != '\n') {
        uint16_t keycode = BADUSB_ASCII_TO_KEY(bad_usb, print_char);
        if(keycode != HID_KEYBOARD_NONE) {
            bad_usb->hid->kb_press(bad_usb->hid_inst, keycode);
            bad_usb->hid->kb_release(bad_usb->hid_inst, keycode);
        }
    } else {
        bad_usb->hid->kb_press(bad_usb->hid_inst, HID_KEYBOARD_RETURN);
        bad_usb->hid->kb_release(bad_usb->hid_inst, HID_KEYBOARD_RETURN);
    }

    bad_usb->string_print_pos++;

    return false;
}

static void bad_usb_hid_state_callback(bool state, void* context) {
    furi_assert(context);
    BadUsbPayload* bad_usb = context;

    if(state == true) {
        furi_thread_flags_set(furi_thread_get_id(bad_usb->thread), WorkerEvtConnect);
    } else {
        furi_thread_flags_set(furi_thread_get_id(bad_usb->thread), WorkerEvtDisconnect);
    }
}

static bool bad_usb_setup_hid(BadUsbPayload* bad_usb) {
    // TODO: set these to something meaningful
    bad_usb->hid_cfg.vid = 0x1234;
    bad_usb->hid_cfg.pid = 0xabcd;
    strncpy(bad_usb->hid_cfg.manuf, "Generic", 32);
    strncpy(bad_usb->hid_cfg.product, "USB Keyboard", 32);

    bad_usb->hid_inst = bad_usb->hid->init(&bad_usb->hid_cfg);
    bad_usb->hid->set_state_callback(bad_usb->hid_inst, bad_usb_hid_state_callback, bad_usb);

    return true;
}

static uint32_t bad_usb_flags_get(uint32_t flags_mask, uint32_t timeout) {
    uint32_t flags = furi_thread_flags_get();
    furi_check((flags & FuriFlagError) == 0);
    if(flags == 0) {
        flags = furi_thread_flags_wait(flags_mask, FuriFlagWaitAny, timeout);
        furi_check(((flags & FuriFlagError) == 0) || (flags == (unsigned)FuriFlagErrorTimeout));
    } else {
        uint32_t state = furi_thread_flags_clear(flags);
        furi_check((state & FuriFlagError) == 0);
    }
    return flags;
}

static int32_t clippy_bad_usb_worker(void* context) {
    BadUsbPayload* bad_usb = context;

    ClippyBadUsbWorkerState worker_state = ClippyBadUsbStateInit;

    FURI_LOG_I(WORKER_TAG, "Init");

    while(1) {
        if(worker_state == ClippyBadUsbStateInit) { // State: initialization
            if(bad_usb_setup_hid(bad_usb)) {
                if(bad_usb->hid->is_connected(bad_usb->hid_inst)) {
                    worker_state = ClippyBadUsbStateIdle; // Ready to run
                } else {
                    worker_state = ClippyBadUsbStateNotConnected; // USB not connected
                }
            } else {
                worker_state = ClippyBadUsbStateInitError; // Init error
            }
            bad_usb->st.state = worker_state;

        } else if(worker_state == ClippyBadUsbStateNotConnected) { // State: USB not connected
            uint32_t flags = bad_usb_flags_get(
                WorkerEvtEnd | WorkerEvtConnect | WorkerEvtDisconnect | WorkerEvtStartStop,
                FuriWaitForever);

            if(flags & WorkerEvtEnd) {
                break;
            } else if(flags & WorkerEvtConnect) {
                worker_state = ClippyBadUsbStateIdle; // Ready to run
            } else if(flags & WorkerEvtStartStop) {
                worker_state = ClippyBadUsbStateWillRun; // Will run when USB is connected
            }
            bad_usb->st.state = worker_state;

        } else if(worker_state == ClippyBadUsbStateIdle) { // State: ready to start
            uint32_t flags = bad_usb_flags_get(
                WorkerEvtEnd | WorkerEvtStartStop | WorkerEvtDisconnect, FuriWaitForever);

            if(flags & WorkerEvtEnd) {
                break;
            } else if(flags & WorkerEvtStartStop) { // Send payload
                dolphin_deed(DolphinDeedBadUsbPlayScript);
                worker_state = ClippyBadUsbStateRunning;
                bad_usb->st.string_idx = bad_usb->string_print_pos = 0;
            } else if(flags & WorkerEvtDisconnect) {
                worker_state = ClippyBadUsbStateNotConnected; // USB disconnected
            }
            bad_usb->st.state = worker_state;

        } else if(worker_state == ClippyBadUsbStateWillRun) { // State: start on connection
            uint32_t flags = bad_usb_flags_get(
                WorkerEvtEnd | WorkerEvtConnect | WorkerEvtStartStop, FuriWaitForever);

            if(flags & WorkerEvtEnd) {
                break;
            } else if(flags & WorkerEvtConnect) { // Start executing script
                dolphin_deed(DolphinDeedBadUsbPlayScript);
                // extra time for PC to recognize Flipper as keyboard
                flags = furi_thread_flags_wait(
                    WorkerEvtEnd | WorkerEvtDisconnect | WorkerEvtStartStop,
                    FuriFlagWaitAny | FuriFlagNoClear,
                    1500);
                if(flags == (unsigned)FuriFlagErrorTimeout) {
                    // If nothing happened - start script execution
                    worker_state = ClippyBadUsbStateRunning;
                } else if(flags & WorkerEvtStartStop) {
                    worker_state = ClippyBadUsbStateIdle;
                    furi_thread_flags_clear(WorkerEvtStartStop);
                }
            } else if(flags & WorkerEvtStartStop) { // Cancel scheduled execution
                worker_state = ClippyBadUsbStateNotConnected;
            }
            bad_usb->st.state = worker_state;

        } else if(worker_state == ClippyBadUsbStateRunning) { // State: running
            uint32_t delay = bad_usb->delay_between_keystrokes;
            uint32_t flags = furi_thread_flags_wait(
                WorkerEvtEnd | WorkerEvtStartStop | WorkerEvtDisconnect, FuriFlagWaitAny, delay);

            if(!(flags & FuriFlagError)) {
                if(flags & WorkerEvtEnd) {
                    break;
                } else if(flags & WorkerEvtStartStop) {
                    worker_state = ClippyBadUsbStateIdle; // Stop sending payload
                    bad_usb->hid->release_all(bad_usb->hid_inst);
                } else if(flags & WorkerEvtDisconnect) {
                    worker_state = ClippyBadUsbStateNotConnected; // USB disconnected
                    bad_usb->hid->release_all(bad_usb->hid_inst);
                }
                bad_usb->st.state = worker_state;
                continue;
            } else if(
                (flags == (unsigned)FuriFlagErrorTimeout) ||
                (flags == (unsigned)FuriFlagErrorResource)) {
                // TODO: check error
                // ducky_string(bad_usb, furi_string_get_cstr(bad_usb->string_print));
                if(ducky_string_next(bad_usb)) {
                    worker_state = ClippyBadUsbStateIdle;
                    bad_usb->st.state = ClippyBadUsbStateDone;
                    bad_usb->hid->release_all(bad_usb->hid_inst);
                    continue;
                } else {
                    bad_usb->st.string_idx = bad_usb->string_print_pos;
                }
            } else {
                furi_check((flags & FuriFlagError) == 0);
            }
        } else if(worker_state == ClippyBadUsbStateWaitForBtn) { // State: Wait for button Press
            uint32_t flags = bad_usb_flags_get(
                WorkerEvtEnd | WorkerEvtStartStop | WorkerEvtDisconnect, FuriWaitForever);
            if(!(flags & FuriFlagError)) {
                if(flags & WorkerEvtEnd) {
                    break;
                } else if(flags & WorkerEvtStartStop) {
                    worker_state = ClippyBadUsbStateRunning;
                } else if(flags & WorkerEvtDisconnect) {
                    worker_state = ClippyBadUsbStateNotConnected; // USB disconnected
                    bad_usb->hid->release_all(bad_usb->hid_inst);
                }
                bad_usb->st.state = worker_state;
                continue;
            }
        } else if(worker_state == ClippyBadUsbStateInitError) { // State: error
            uint32_t flags =
                bad_usb_flags_get(WorkerEvtEnd, FuriWaitForever); // Waiting for exit command

            if(flags & WorkerEvtEnd) {
                break;
            }
        }
    }

    bad_usb->hid->set_state_callback(bad_usb->hid_inst, NULL, NULL);
    bad_usb->hid->deinit(bad_usb->hid_inst);

    furi_string_free(bad_usb->string_print);

    FURI_LOG_I(WORKER_TAG, "End");

    return 0;
}

static void clippy_bad_usb_payload_set_default_keyboard_layout(BadUsbPayload* bad_usb) {
    furi_assert(bad_usb);
    memset(bad_usb->layout, HID_KEYBOARD_NONE, sizeof(bad_usb->layout));
    memcpy(bad_usb->layout, hid_asciimap, MIN(sizeof(hid_asciimap), sizeof(bad_usb->layout)));
}

BadUsbPayload* clippy_bad_usb_payload_setup(
    FuriString* string_to_print,
    BadUsbHidInterface interface,
    uint32_t delay) {
    furi_assert(string_to_print);

    BadUsbPayload* bad_usb = malloc(sizeof(BadUsbPayload));
    bad_usb->delay_between_keystrokes = 25 * pow(2, delay);
    bad_usb->string_print = furi_string_alloc();
    furi_string_set(bad_usb->string_print, string_to_print);
    clippy_bad_usb_payload_set_default_keyboard_layout(bad_usb);

    bad_usb->st.state = ClippyBadUsbStateInit;
    bad_usb->st.string_length = furi_string_size(bad_usb->string_print);
    bad_usb->st.string_idx = 0;
    bad_usb->st.error[0] = '\0';
    bad_usb->hid = bad_usb_hid_get_interface(interface);

    bad_usb->thread = furi_thread_alloc_ex("BadUsbWorker", 2048, clippy_bad_usb_worker, bad_usb);
    furi_thread_start(bad_usb->thread);
    return bad_usb;
} //-V773

void clippy_bad_usb_payload_teardown(BadUsbPayload* bad_usb) {
    furi_assert(bad_usb);
    furi_thread_flags_set(furi_thread_get_id(bad_usb->thread), WorkerEvtEnd);
    furi_thread_join(bad_usb->thread);
    furi_thread_free(bad_usb->thread);
    furi_string_free(bad_usb->string_print);
    free(bad_usb);
}

void clippy_bad_usb_payload_set_keyboard_layout(BadUsbPayload* bad_usb, FuriString* layout_path) {
    furi_assert(bad_usb);

    if(bad_usb->st.state == ClippyBadUsbStateRunning) {
        // do not update keyboard layout while a script is running
        return;
    }

    File* layout_file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(!furi_string_empty(layout_path)) { //-V1051
        if(storage_file_open(
               layout_file, furi_string_get_cstr(layout_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint16_t layout[128];
            if(storage_file_read(layout_file, layout, sizeof(layout)) == sizeof(layout)) {
                memcpy(bad_usb->layout, layout, sizeof(layout));
            }
        }
        storage_file_close(layout_file);
    } else {
        clippy_bad_usb_payload_set_default_keyboard_layout(bad_usb);
    }
    storage_file_free(layout_file);
}

void clippy_bad_usb_payload_start_stop(BadUsbPayload* bad_usb) {
    furi_assert(bad_usb);
    furi_thread_flags_set(furi_thread_get_id(bad_usb->thread), WorkerEvtStartStop);
}

ClippyBadUsbState* clippy_bad_usb_payload_get_state(BadUsbPayload* bad_usb) {
    furi_assert(bad_usb);
    return &(bad_usb->st);
}
