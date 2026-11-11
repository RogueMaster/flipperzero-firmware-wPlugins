#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include "ducky_script.h"
#include "ducky_script_3.h"
#include "bad_usb_hid.h"

#define SCRIPT_STATE_ERROR        (-1)
#define SCRIPT_STATE_END          (-2)
#define SCRIPT_STATE_NEXT_LINE    (-3)
#define SCRIPT_STATE_CMD_UNKNOWN  (-4)
#define SCRIPT_STATE_STRING_START (-5)
#define SCRIPT_STATE_WAIT_FOR_BTN (-6)
#define SCRIPT_STATE_JUMP         (-7)

#define FILE_BUFFER_LEN 16

#define HID_MOUSE_INVALID 0
#define HID_MOUSE_NONE    0

struct BadUsbScript {
    BadUsbHidInterface* interface;
    BadUsbHidConfig* hid_cfg;
    bool load_id_cfg;
    const BadUsbHidApi* hid;
    void* hid_inst;
    FuriThread* thread;
    BadUsbState st;

    FuriString* file_path;
    uint8_t file_buf[FILE_BUFFER_LEN + 1];
    uint8_t buf_start;
    uint8_t buf_len;
    bool file_end;

    uint32_t defdelay;
    uint32_t stringdelay;
    uint32_t defstringdelay;
    uint16_t layout[128];

    FuriString* line;
    FuriString* line_prev;
    uint32_t repeat_cnt;
    uint8_t key_hold_nb;

    FuriString* string_print;
    size_t string_print_pos;
    Ducky3Context ducky3;

    // USB Mass-Storage mount state (MASS_STORAGE_MOUNT). While mounted, HID is
    // torn down (single-function USB) and hid_inst is NULL; UMOUNT restores HID.
    void* msc; // MassStorageUsb* while mounted, else NULL
    File* msc_file; // backing image file while mounted
    Storage* msc_storage; // storage record held while mounted
};

uint16_t ducky_get_keycode(BadUsbScript* bad_usb, const char* param, bool accept_modifiers);

uint32_t ducky_get_command_len(const char* line);

bool ducky_is_line_end(const char chr);

uint16_t ducky_get_next_modifier_keycode_by_name(const char** param);

uint16_t ducky_get_modifier_keycode_by_name(const char* param);

uint16_t ducky_get_keycode_by_name(const char* param);

uint16_t ducky_get_media_keycode_by_name(const char* param);

uint8_t ducky_get_mouse_keycode_by_name(const char* param);

bool ducky_get_number(const char* param, uint32_t* val);

void ducky_numlock_on(BadUsbScript* bad_usb);

bool ducky_numpad_press(BadUsbScript* bad_usb, const char num);

bool ducky_altchar(BadUsbScript* bad_usb, const char* charcode);

bool ducky_altstring(BadUsbScript* bad_usb, const char* param);

bool ducky_string(BadUsbScript* bad_usb, const char* param);

int32_t ducky_execute_cmd(BadUsbScript* bad_usb, const char* line);

int32_t ducky_error(BadUsbScript* bad_usb, const char* text, ...);

// Mass-Storage (MASS_STORAGE_MOUNT/UMOUNT). Defined in ducky_script.c because
// mount/umount re-register the file-scope HID state callback there.
bool bad_usb_msc_mount(BadUsbScript* bad_usb, const char* path);

void bad_usb_msc_umount(BadUsbScript* bad_usb);

bool bad_usb_msc_is_mounted(BadUsbScript* bad_usb);

#ifdef __cplusplus
}
#endif
