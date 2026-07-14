#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include <furi_hal.h>
#include "ducky_script.h"
#include "bad_usb_hid.h"

#define SCRIPT_STATE_ERROR        (-1)
#define SCRIPT_STATE_END          (-2)
#define SCRIPT_STATE_NEXT_LINE    (-3)
#define SCRIPT_STATE_CMD_UNKNOWN  (-4)
#define SCRIPT_STATE_STRING_START (-5)
#define SCRIPT_STATE_WAIT_FOR_BTN (-6)

#define FILE_BUFFER_LEN 16

struct BadUsbPayload {
    FuriHalUsbHidConfig hid_cfg;
    const BadUsbHidApi* hid;
    void* hid_inst;
    FuriThread* thread;
    ClippyBadUsbState st;

    FuriString* string_print;
    size_t string_print_pos;
    uint32_t delay_between_keystrokes;

    uint16_t layout[128];
};

uint16_t ducky_get_keycode(BadUsbPayload* bad_usb, const char* param, bool accept_chars);

uint32_t ducky_get_command_len(const char* line);

bool ducky_is_line_end(const char chr);

uint16_t ducky_get_keycode_by_name(const char* param);

uint16_t ducky_get_media_keycode_by_name(const char* param);

bool ducky_get_number(const char* param, uint32_t* val);

void ducky_numlock_on(BadUsbPayload* bad_usb);

bool ducky_numpad_press(BadUsbPayload* bad_usb, const char num);

bool ducky_altchar(BadUsbPayload* bad_usb, const char* charcode);

bool ducky_altstring(BadUsbPayload* bad_usb, const char* param);

bool ducky_string(BadUsbPayload* bad_usb, const char* param);

int32_t ducky_execute_cmd(BadUsbPayload* bad_usb, const char* line);

int32_t ducky_error(BadUsbPayload* bad_usb, const char* text, ...);

#ifdef __cplusplus
}
#endif
