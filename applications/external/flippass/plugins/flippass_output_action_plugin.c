#include "flippass_output_action_plugin.h"

#include "../hid_keys.h"

#include <furi_hal.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_BLE_PRESS_DELAY_MS              12U
#define FLIPPASS_BLE_RELEASE_DELAY_MS            18U
#define FLIPPASS_BLE_STEP_DELAY_MS               45U
#define FLIPPASS_USB_PRESS_DELAY_MS              12U
#define FLIPPASS_USB_RELEASE_DELAY_MS            18U
#define FLIPPASS_USB_STEP_DELAY_MS               45U
#define FLIPPASS_OUTPUT_PRE_PRESS_DELAY_MS       10U
#define FLIPPASS_OUTPUT_ALT_PRE_PRESS_DELAY_MS   30U
#define FLIPPASS_OUTPUT_KEEPASS_DEFAULT_SEQUENCE "{USERNAME}{TAB}{PASSWORD}{ENTER}"
#define FLIPPASS_OUTPUT_CLEARFIELD_SEQUENCE      "{HOME}+({END}){BKSP}{DELAY 50}"

typedef struct {
    const char* token;
    uint16_t hid_key;
} FlipPassOutputSpecialKey;

typedef struct {
    const FlipPassOutputActionRequestV1* request;
    const FlipPassOutputActionHostApiV1* host_api;
    FlipPassOutputActionPluginTransport transport;
    uint16_t sticky_modifiers;
    uint16_t current_modifiers;
    uint32_t default_delay_ms;
    uint16_t layout[128];
    size_t progress_total;
    const char* progress_detail;
    FuriString* placeholder_buffer;
    uint8_t progress_percent;
    bool use_alt_numpad;
    bool pending_cr;
} FlipPassOutputSession;

typedef enum {
    FlipPassOutputLiteralModePlain = 0,
    FlipPassOutputLiteralModeAltNumpad,
} FlipPassOutputLiteralMode;

static const uint8_t flippass_output_numpad_keys[10] = {
    HID_KEYPAD_0,
    HID_KEYPAD_1,
    HID_KEYPAD_2,
    HID_KEYPAD_3,
    HID_KEYPAD_4,
    HID_KEYPAD_5,
    HID_KEYPAD_6,
    HID_KEYPAD_7,
    HID_KEYPAD_8,
    HID_KEYPAD_9,
};

static bool flippass_output_session_prepare_alt_numpad(FlipPassOutputSession* session);
static bool flippass_output_session_cancel_requested(const FlipPassOutputSession* session);
static bool flippass_output_session_delay(FlipPassOutputSession* session, uint32_t delay_ms);
static bool
    flippass_output_session_type_alt_code_byte(FlipPassOutputSession* session, uint8_t value);
static bool flippass_output_session_type_text_byte(FlipPassOutputSession* session, uint8_t value);
static bool flippass_output_session_flush_pending_cr(FlipPassOutputSession* session);
static bool flippass_output_session_stream_text_chunk(
    FlipPassOutputSession* session,
    const uint8_t* data,
    size_t data_size,
    size_t* completed_steps);

static void flippass_output_progress_begin(
    FlipPassOutputSession* session,
    size_t total_steps,
    const char* detail) {
    if(session == NULL || session->host_api == NULL || session->host_api->progress == NULL) {
        return;
    }

    session->progress_total = (total_steps > 0U) ? total_steps : 1U;
    session->progress_detail = detail;
    session->progress_percent = 45U;
    session->host_api->progress(session->host_api->context, "Typing", detail, 45U);
}

static bool flippass_output_load_layout_file(FlipPassOutputSession* session) {
    if(session == NULL || session->request == NULL ||
       session->request->keyboard_layout_path == NULL ||
       session->request->keyboard_layout_path[0] == '\0') {
        return false;
    }

    bool loaded = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* layout_file = storage_file_alloc(storage);
    const char* layout_path = session->request->keyboard_layout_path;

    if(storage_file_open(layout_file, layout_path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_read(layout_file, session->layout, sizeof(session->layout)) ==
           sizeof(session->layout)) {
        loaded = true;
    }

    storage_file_close(layout_file);
    storage_file_free(layout_file);
    furi_record_close(RECORD_STORAGE);
    return loaded;
}

static void
    flippass_output_progress_update(FlipPassOutputSession* session, size_t completed_steps) {
    uint8_t percent = 45U;

    if(session == NULL || session->host_api == NULL || session->host_api->progress == NULL ||
       session->progress_total == 0U) {
        return;
    }

    if(completed_steps > session->progress_total) {
        completed_steps = session->progress_total;
    }

    percent = (uint8_t)(45U + ((completed_steps * 53U) / session->progress_total));
    if(percent > 98U) {
        percent = 98U;
    }
    if(percent <= session->progress_percent && completed_steps < session->progress_total) {
        return;
    }

    session->progress_percent = percent;
    session->host_api->progress(
        session->host_api->context, "Typing", session->progress_detail, percent);
}

static char flippass_output_ascii_upper(char value) {
    if(value >= 'a' && value <= 'z') {
        value = (char)(value - 'a' + 'A');
    }

    return value;
}

static bool
    flippass_output_token_n_equals(const char* token, size_t token_len, const char* expected) {
    if(token == NULL || expected == NULL) {
        return false;
    }

    size_t index = 0U;
    while(index < token_len && expected[index] != '\0') {
        if(flippass_output_ascii_upper(token[index]) !=
           flippass_output_ascii_upper(expected[index])) {
            return false;
        }
        index++;
    }

    return (index == token_len) && (expected[index] == '\0');
}

static bool
    flippass_output_token_n_starts_with(const char* token, size_t token_len, const char* prefix) {
    if(token == NULL || prefix == NULL) {
        return false;
    }

    size_t index = 0U;
    while(prefix[index] != '\0') {
        if(index >= token_len) {
            return false;
        }

        if(flippass_output_ascii_upper(token[index]) !=
           flippass_output_ascii_upper(prefix[index])) {
            return false;
        }
        index++;
    }

    return true;
}

static bool flippass_output_parse_uint_n(const char* text, size_t text_len, uint32_t* value) {
    unsigned long parsed = 0UL;

    if(text == NULL || value == NULL || text_len == 0U) {
        return false;
    }

    for(size_t index = 0U; index < text_len; index++) {
        if(text[index] < '0' || text[index] > '9') {
            return false;
        }

        if(parsed < 60000UL) {
            parsed = parsed * 10UL + (unsigned long)(text[index] - '0');
            if(parsed > 60000UL) {
                parsed = 60000UL;
            }
        }
    }

    *value = (uint32_t)parsed;
    return true;
}

static void flippass_output_cat_strn(FuriString* out, const char* text, size_t len) {
    if(out == NULL || text == NULL || len == 0U) {
        return;
    }

    furi_string_cat_printf(out, "%.*s", (int)len, text);
}

static const FlipPassOutputSpecialKey flippass_output_special_keys[] = {
    {"TAB", HID_KEYBOARD_TAB},
    {"ENTER", HID_KEYBOARD_RETURN},
    {"RETURN", HID_KEYBOARD_RETURN},
    {"UP", HID_KEYBOARD_UP_ARROW},
    {"DOWN", HID_KEYBOARD_DOWN_ARROW},
    {"LEFT", HID_KEYBOARD_LEFT_ARROW},
    {"RIGHT", HID_KEYBOARD_RIGHT_ARROW},
    {"INSERT", HID_KEYBOARD_INSERT},
    {"INS", HID_KEYBOARD_INSERT},
    {"DELETE", HID_KEYBOARD_DELETE_FORWARD},
    {"DEL", HID_KEYBOARD_DELETE_FORWARD},
    {"HOME", HID_KEYBOARD_HOME},
    {"END", HID_KEYBOARD_END},
    {"PGUP", HID_KEYBOARD_PAGE_UP},
    {"PGDN", HID_KEYBOARD_PAGE_DOWN},
    {"SPACE", HID_KEYBOARD_SPACEBAR},
    {"BACKSPACE", HID_KEYBOARD_DELETE},
    {"BS", HID_KEYBOARD_DELETE},
    {"BKSP", HID_KEYBOARD_DELETE},
    {"BREAK", HID_KEYBOARD_PAUSE},
    {"CAPSLOCK", HID_KEYBOARD_CAPS_LOCK},
    {"CLEAR", HID_KEYBOARD_CLEAR},
    {"ESC", HID_KEYBOARD_ESCAPE},
    {"ESCAPE", HID_KEYBOARD_ESCAPE},
    {"WIN", KEY_MOD_LEFT_GUI},
    {"LWIN", KEY_MOD_LEFT_GUI},
    {"RWIN", KEY_MOD_RIGHT_GUI},
    {"APPS", HID_KEYBOARD_APPLICATION},
    {"HELP", HID_KEYBOARD_HELP},
    {"NUMLOCK", HID_KEYPAD_NUMLOCK},
    {"PRTSC", HID_KEYBOARD_PRINT_SCREEN},
    {"SCROLLLOCK", HID_KEYBOARD_SCROLL_LOCK},
    {"F1", HID_KEYBOARD_F1},
    {"F2", HID_KEYBOARD_F2},
    {"F3", HID_KEYBOARD_F3},
    {"F4", HID_KEYBOARD_F4},
    {"F5", HID_KEYBOARD_F5},
    {"F6", HID_KEYBOARD_F6},
    {"F7", HID_KEYBOARD_F7},
    {"F8", HID_KEYBOARD_F8},
    {"F9", HID_KEYBOARD_F9},
    {"F10", HID_KEYBOARD_F10},
    {"F11", HID_KEYBOARD_F11},
    {"F12", HID_KEYBOARD_F12},
    {"F13", HID_KEYBOARD_F13},
    {"F14", HID_KEYBOARD_F14},
    {"F15", HID_KEYBOARD_F15},
    {"F16", HID_KEYBOARD_F16},
    {"F17", HID_KEYBOARD_F17},
    {"F18", HID_KEYBOARD_F18},
    {"F19", HID_KEYBOARD_F19},
    {"F20", HID_KEYBOARD_F20},
    {"F21", HID_KEYBOARD_F21},
    {"F22", HID_KEYBOARD_F22},
    {"F23", HID_KEYBOARD_F23},
    {"F24", HID_KEYBOARD_F24},
    {"ADD", HID_KEYPAD_PLUS},
    {"SUBTRACT", HID_KEYPAD_MINUS},
    {"MULTIPLY", HID_KEYPAD_ASTERISK},
    {"DIVIDE", HID_KEYPAD_SLASH},
    {"NUMPAD0", HID_KEYPAD_0},
    {"NUMPAD1", HID_KEYPAD_1},
    {"NUMPAD2", HID_KEYPAD_2},
    {"NUMPAD3", HID_KEYPAD_3},
    {"NUMPAD4", HID_KEYPAD_4},
    {"NUMPAD5", HID_KEYPAD_5},
    {"NUMPAD6", HID_KEYPAD_6},
    {"NUMPAD7", HID_KEYPAD_7},
    {"NUMPAD8", HID_KEYPAD_8},
    {"NUMPAD9", HID_KEYPAD_9},
    {"PLUS", HID_KEYBOARD_EQUAL_SIGN | KEY_MOD_LEFT_SHIFT},
    {"PERCENT", HID_KEYBOARD_5 | KEY_MOD_LEFT_SHIFT},
    {"CARET", HID_KEYBOARD_6 | KEY_MOD_LEFT_SHIFT},
    {"TILDE", HID_KEYBOARD_GRAVE_ACCENT | KEY_MOD_LEFT_SHIFT},
    {"LEFTPAREN", HID_KEYBOARD_9 | KEY_MOD_LEFT_SHIFT},
    {"RIGHTPAREN", HID_KEYBOARD_0 | KEY_MOD_LEFT_SHIFT},
    {"LEFTBRACKET", HID_KEYBOARD_OPEN_BRACKET},
    {"RIGHTBRACKET", HID_KEYBOARD_CLOSE_BRACKET},
    {"LEFTBRACE", HID_KEYBOARD_OPEN_BRACKET | KEY_MOD_LEFT_SHIFT},
    {"RIGHTBRACE", HID_KEYBOARD_CLOSE_BRACKET | KEY_MOD_LEFT_SHIFT},
};

static const FlipPassOutputSpecialKey*
    flippass_output_find_special_key_n(const char* token, size_t token_len) {
    for(size_t index = 0U; index < COUNT_OF(flippass_output_special_keys); index++) {
        if(flippass_output_token_n_equals(
               token, token_len, flippass_output_special_keys[index].token)) {
            return &flippass_output_special_keys[index];
        }
    }

    return NULL;
}

static uint16_t flippass_output_modifier_from_symbol(char symbol) {
    switch(symbol) {
    case '+':
        return KEY_MOD_LEFT_SHIFT;
    case '^':
        return KEY_MOD_LEFT_CTRL;
    case '%':
        return KEY_MOD_LEFT_ALT;
    default:
        return 0U;
    }
}

static bool
    flippass_output_session_press_prepared(FlipPassOutputSession* session, uint16_t hid_key) {
    furi_assert(session);
    return session->host_api != NULL && session->host_api->press_key != NULL &&
           session->host_api->press_key(session->host_api->context, session->transport, hid_key);
}

static bool
    flippass_output_session_release_prepared(FlipPassOutputSession* session, uint16_t hid_key) {
    furi_assert(session);
    return session->host_api != NULL && session->host_api->release_key != NULL &&
           session->host_api->release_key(session->host_api->context, session->transport, hid_key);
}

static void flippass_output_session_release_all_prepared(FlipPassOutputSession* session) {
    furi_assert(session);
    if(session->host_api != NULL && session->host_api->release_all != NULL) {
        session->host_api->release_all(session->host_api->context, session->transport);
    }
}

static bool flippass_output_session_begin(FlipPassOutputSession* session) {
    furi_assert(session);

    if(flippass_output_session_cancel_requested(session)) {
        return false;
    }

    session->sticky_modifiers = 0U;
    session->current_modifiers = 0U;
    session->pending_cr = false;
    session->use_alt_numpad = session->request == NULL ||
                              session->request->keyboard_layout_path == NULL ||
                              session->request->keyboard_layout_path[0] == '\0';
    session->default_delay_ms =
        (session->transport == FlipPassOutputActionPluginTransportBluetooth) ?
            FLIPPASS_BLE_STEP_DELAY_MS :
            FLIPPASS_USB_STEP_DELAY_MS;

    if(!session->use_alt_numpad && !flippass_output_load_layout_file(session)) {
        return false;
    }

    if(session->host_api == NULL || session->host_api->begin_transport == NULL ||
       !session->host_api->begin_transport(session->host_api->context, session->transport)) {
        return false;
    }

    if(session->use_alt_numpad && !flippass_output_session_prepare_alt_numpad(session)) {
        return false;
    }

    return true;
}

static void flippass_output_session_end(FlipPassOutputSession* session) {
    furi_assert(session);

    if(flippass_output_session_cancel_requested(session)) {
        if(session->host_api != NULL && session->host_api->progress != NULL) {
            session->host_api->progress(
                session->host_api->context, "Canceling", "Cleaning up.", 99U);
        }
    }

    flippass_output_session_release_all_prepared(session);
    session->sticky_modifiers = 0U;
    session->current_modifiers = 0U;
    session->pending_cr = false;
    if(session->host_api != NULL && session->host_api->end_transport != NULL) {
        session->host_api->end_transport(session->host_api->context, session->transport);
    }
}

static bool flippass_output_session_cancel_requested(const FlipPassOutputSession* session) {
    return session != NULL && session->host_api != NULL &&
           session->host_api->should_cancel != NULL &&
           session->host_api->should_cancel(session->host_api->context);
}

static bool flippass_output_session_delay(FlipPassOutputSession* session, uint32_t delay_ms) {
    while(delay_ms > 0U) {
        const uint32_t step_ms = (delay_ms > 25U) ? 25U : delay_ms;

        if(flippass_output_session_cancel_requested(session)) {
            return false;
        }

        furi_delay_ms(step_ms);
        delay_ms -= step_ms;
    }

    return !flippass_output_session_cancel_requested(session);
}

static bool flippass_output_session_inter_key_delay(FlipPassOutputSession* session) {
    furi_assert(session);
    return flippass_output_session_delay(session, session->default_delay_ms);
}

static bool
    flippass_output_session_pre_press_delay(FlipPassOutputSession* session, uint32_t delay_ms) {
    return flippass_output_session_delay(session, delay_ms);
}

static bool flippass_output_session_press_modifier_mask(
    FlipPassOutputSession* session,
    uint16_t modifier_mask) {
    static const uint16_t modifier_order[] = {
        KEY_MOD_LEFT_CTRL,
        KEY_MOD_LEFT_SHIFT,
        KEY_MOD_LEFT_ALT,
        KEY_MOD_LEFT_GUI,
        KEY_MOD_RIGHT_CTRL,
        KEY_MOD_RIGHT_SHIFT,
        KEY_MOD_RIGHT_ALT,
        KEY_MOD_RIGHT_GUI,
    };

    for(size_t index = 0U; index < COUNT_OF(modifier_order); index++) {
        const uint16_t modifier = modifier_order[index];
        if((modifier_mask & modifier) == 0U || (session->current_modifiers & modifier) != 0U) {
            continue;
        }
        const uint32_t pre_press_delay =
            (modifier == KEY_MOD_LEFT_ALT || modifier == KEY_MOD_RIGHT_ALT) ?
                FLIPPASS_OUTPUT_ALT_PRE_PRESS_DELAY_MS :
                FLIPPASS_OUTPUT_PRE_PRESS_DELAY_MS;
        if(!flippass_output_session_pre_press_delay(session, pre_press_delay)) {
            return false;
        }
        if(!flippass_output_session_press_prepared(session, modifier)) {
            return false;
        }
        session->current_modifiers |= modifier;
        if(!flippass_output_session_inter_key_delay(session)) {
            return false;
        }
    }

    return true;
}

static bool flippass_output_session_release_modifier_mask(
    FlipPassOutputSession* session,
    uint16_t modifier_mask) {
    static const uint16_t modifier_order[] = {
        KEY_MOD_RIGHT_GUI,
        KEY_MOD_RIGHT_ALT,
        KEY_MOD_RIGHT_SHIFT,
        KEY_MOD_RIGHT_CTRL,
        KEY_MOD_LEFT_GUI,
        KEY_MOD_LEFT_ALT,
        KEY_MOD_LEFT_SHIFT,
        KEY_MOD_LEFT_CTRL,
    };

    for(size_t index = 0U; index < COUNT_OF(modifier_order); index++) {
        const uint16_t modifier = modifier_order[index];
        if((modifier_mask & modifier) == 0U || (session->current_modifiers & modifier) == 0U) {
            continue;
        }
        if(!flippass_output_session_release_prepared(session, modifier)) {
            return false;
        }
        session->current_modifiers &= ~modifier;
        if(!flippass_output_session_inter_key_delay(session)) {
            return false;
        }
    }

    return true;
}

static bool flippass_output_session_set_modifiers(
    FlipPassOutputSession* session,
    uint16_t parser_modifiers) {
    const uint16_t wanted_modifiers = parser_modifiers | session->sticky_modifiers;
    const uint16_t release_mask = session->current_modifiers & ~wanted_modifiers;
    const uint16_t press_mask = wanted_modifiers & ~session->current_modifiers;

    return flippass_output_session_release_modifier_mask(session, release_mask) &&
           flippass_output_session_press_modifier_mask(session, press_mask);
}

static bool flippass_output_session_tap_raw_key(
    FlipPassOutputSession* session,
    uint16_t base_key,
    uint16_t extra_modifiers) {
    const uint16_t temp_modifiers = extra_modifiers & ~session->current_modifiers;
    const uint32_t press_delay =
        (session->transport == FlipPassOutputActionPluginTransportBluetooth) ?
            FLIPPASS_BLE_PRESS_DELAY_MS :
            FLIPPASS_USB_PRESS_DELAY_MS;
    const uint32_t release_delay =
        (session->transport == FlipPassOutputActionPluginTransportBluetooth) ?
            FLIPPASS_BLE_RELEASE_DELAY_MS :
            FLIPPASS_USB_RELEASE_DELAY_MS;

    if(base_key == HID_KEYBOARD_NONE || flippass_output_session_cancel_requested(session)) {
        return false;
    }

    if(temp_modifiers != 0U &&
       !flippass_output_session_press_modifier_mask(session, temp_modifiers)) {
        return false;
    }

    if(!flippass_output_session_pre_press_delay(session, FLIPPASS_OUTPUT_PRE_PRESS_DELAY_MS)) {
        return false;
    }
    if(!flippass_output_session_press_prepared(session, base_key)) {
        if(temp_modifiers != 0U) {
            flippass_output_session_release_modifier_mask(session, temp_modifiers);
        }
        return false;
    }

    if(!flippass_output_session_delay(session, press_delay)) {
        flippass_output_session_release_all_prepared(session);
        return false;
    }

    if(!flippass_output_session_release_prepared(session, base_key)) {
        flippass_output_session_release_all_prepared(session);
        return false;
    }

    if(!flippass_output_session_delay(session, release_delay)) {
        return false;
    }

    if(temp_modifiers != 0U &&
       !flippass_output_session_release_modifier_mask(session, temp_modifiers)) {
        return false;
    }

    if(!flippass_output_session_inter_key_delay(session)) {
        return false;
    }
    if(base_key == HID_KEYBOARD_TAB && session->default_delay_ms < 100U &&
       !flippass_output_session_delay(session, session->default_delay_ms)) {
        return false;
    }

    return true;
}

static bool flippass_output_session_tap_key(FlipPassOutputSession* session, uint16_t hid_key) {
    return flippass_output_session_tap_raw_key(session, hid_key & 0xFFU, hid_key & 0xFF00U);
}

static bool flippass_output_session_tap_char(FlipPassOutputSession* session, char ch) {
    uint16_t hid_key = HID_KEYBOARD_NONE;

    if(session->use_alt_numpad &&
       (session->current_modifiers & ~(KEY_MOD_LEFT_ALT | KEY_MOD_RIGHT_ALT)) == 0U) {
        return flippass_output_session_type_alt_code_byte(session, (uint8_t)ch);
    }

    if(!session->use_alt_numpad) {
        const uint8_t index = (uint8_t)ch;
        if(index >= COUNT_OF(session->layout)) {
            return false;
        }
        hid_key = session->layout[index];
    } else {
        hid_key = HID_ASCII_TO_KEY(ch);
    }

    if(hid_key == HID_KEYBOARD_NONE) {
        return false;
    }

    return flippass_output_session_tap_key(session, hid_key);
}

static bool flippass_output_session_flush_pending_cr(FlipPassOutputSession* session) {
    if(session == NULL || !session->pending_cr) {
        return true;
    }

    session->pending_cr = false;
    return flippass_output_session_tap_key(session, HID_KEYBOARD_RETURN);
}

static bool flippass_output_session_type_text_byte(FlipPassOutputSession* session, uint8_t value) {
    if(flippass_output_session_cancel_requested(session)) {
        return false;
    }

    if(session->pending_cr) {
        if(value == '\n') {
            session->pending_cr = false;
            return flippass_output_session_tap_key(session, HID_KEYBOARD_RETURN);
        }

        if(!flippass_output_session_flush_pending_cr(session)) {
            return false;
        }
    }

    switch(value) {
    case '\r':
        session->pending_cr = true;
        return true;
    case '\n':
        return flippass_output_session_tap_key(session, HID_KEYBOARD_RETURN);
    case '\t':
        return flippass_output_session_tap_key(session, HID_KEYBOARD_TAB);
    case '\b':
        return flippass_output_session_tap_key(session, HID_KEYBOARD_DELETE);
    default:
        return flippass_output_session_tap_char(session, (char)value);
    }
}

static bool
    flippass_output_session_type_literal(FlipPassOutputSession* session, const char* text) {
    for(size_t index = 0U; text[index] != '\0'; index++) {
        if(!flippass_output_session_type_text_byte(session, (uint8_t)text[index])) {
            return false;
        }
        flippass_output_progress_update(session, index + 1U);
    }

    return flippass_output_session_flush_pending_cr(session);
}

static bool flippass_output_session_stream_cstr(
    FlipPassOutputSession* session,
    const char* text,
    size_t* completed_steps) {
    if(session == NULL || text == NULL || completed_steps == NULL) {
        return false;
    }

    const bool ok = flippass_output_session_stream_text_chunk(
        session, (const uint8_t*)text, strlen(text), completed_steps);
    return ok && flippass_output_session_flush_pending_cr(session);
}

static bool flippass_output_session_stream_text_chunk(
    FlipPassOutputSession* session,
    const uint8_t* data,
    size_t data_size,
    size_t* completed_steps) {
    if(session == NULL || completed_steps == NULL) {
        return false;
    }

    if(data == NULL) {
        return data_size == 0U;
    }

    for(size_t index = 0U; index < data_size; index++) {
        if(!flippass_output_session_type_text_byte(session, data[index])) {
            return false;
        }
        (*completed_steps)++;
        flippass_output_progress_update(session, *completed_steps);
    }

    return true;
}

typedef struct {
    FlipPassOutputSession* session;
    size_t* completed_steps;
} FlipPassOutputRefStreamContext;

static bool
    flippass_output_ref_stream_callback(const uint8_t* data, size_t data_size, void* context) {
    FlipPassOutputRefStreamContext* stream = context;
    if(stream == NULL || stream->session == NULL || stream->completed_steps == NULL) {
        return false;
    }

    return flippass_output_session_stream_text_chunk(
        stream->session, data, data_size, stream->completed_steps);
}

static bool flippass_output_session_stream_vault_ref(
    FlipPassOutputSession* session,
    FlipPassOutputActionPluginRef ref,
    size_t plain_len,
    size_t* completed_steps) {
    furi_assert(session);
    furi_assert(completed_steps);

    if(plain_len == 0U) {
        return true;
    }

    if(session->host_api == NULL || session->host_api->stream_ref == NULL) {
        return false;
    }

    FlipPassOutputRefStreamContext stream = {
        .session = session,
        .completed_steps = completed_steps,
    };
    bool ok = session->host_api->stream_ref(
        session->host_api->context, ref, flippass_output_ref_stream_callback, &stream);
    if(ok) {
        ok = flippass_output_session_flush_pending_cr(session);
    }
    return ok;
}

static uint16_t flippass_output_numpad_key_from_digit(char digit) {
    return (digit >= '0' && digit <= '9') ? flippass_output_numpad_keys[digit - '0'] :
                                            HID_KEYBOARD_NONE;
}

static bool flippass_output_session_prepare_alt_numpad(FlipPassOutputSession* session) {
    if(session->transport != FlipPassOutputActionPluginTransportUsb) {
        return true;
    }

    if(session->host_api != NULL && session->host_api->usb_numlock_on != NULL &&
       session->host_api->usb_numlock_on(session->host_api->context)) {
        return true;
    }

    return flippass_output_session_tap_raw_key(session, HID_KEYBOARD_LOCK_NUM_LOCK, 0U);
}

static bool
    flippass_output_session_type_alt_code_byte(FlipPassOutputSession* session, uint8_t value) {
    char ascii_code[4];
    const bool alt_already_pressed = (session->current_modifiers & KEY_MOD_LEFT_ALT) != 0U;

    snprintf(ascii_code, sizeof(ascii_code), "%u", (unsigned int)value);

    if(!alt_already_pressed &&
       !flippass_output_session_press_modifier_mask(session, KEY_MOD_LEFT_ALT)) {
        return false;
    }

    for(size_t index = 0U; ascii_code[index] != '\0'; index++) {
        const uint16_t numpad_key = flippass_output_numpad_key_from_digit(ascii_code[index]);
        if(numpad_key == HID_KEYBOARD_NONE ||
           !flippass_output_session_tap_raw_key(session, numpad_key, 0U)) {
            if(!alt_already_pressed) {
                flippass_output_session_release_modifier_mask(session, KEY_MOD_LEFT_ALT);
            }
            return false;
        }
    }

    if(!alt_already_pressed &&
       !flippass_output_session_release_modifier_mask(session, KEY_MOD_LEFT_ALT)) {
        return false;
    }

    return true;
}

static bool flippass_output_session_tap_modifier_only(
    FlipPassOutputSession* session,
    uint16_t modifier_mask) {
    return modifier_mask != 0U &&
           flippass_output_session_press_modifier_mask(session, modifier_mask) &&
           flippass_output_session_release_modifier_mask(session, modifier_mask);
}

static void flippass_output_append_url_component(
    FuriString* out,
    const char* url,
    const char* component,
    size_t component_len) {
    const char* scheme_sep = NULL;
    const char* remainder = NULL;
    const char* authority = NULL;
    const char* authority_end = NULL;
    const char* path = NULL;
    const char* query = NULL;
    const char* userinfo = NULL;
    const char* host = NULL;
    const char* port = NULL;
    const char* at_sign = NULL;
    const char* host_end = NULL;
    const char* port_sep = NULL;

    if(out == NULL || component == NULL || component_len == 0U || url == NULL || url[0] == '\0') {
        return;
    }

    scheme_sep = strchr(url, ':');
    remainder = (scheme_sep != NULL) ? (scheme_sep + 1) : url;
    if(scheme_sep != NULL && remainder[0] == '/' && remainder[1] == '/') {
        authority = remainder + 2;
    }

    if(authority != NULL) {
        authority_end = authority;
        while(*authority_end != '\0' && *authority_end != '/' && *authority_end != '?' &&
              *authority_end != '#') {
            authority_end++;
        }

        for(const char* cursor = authority; cursor < authority_end; cursor++) {
            if(*cursor == '@') {
                at_sign = cursor;
            }
        }

        if(at_sign != NULL) {
            userinfo = authority;
            host = at_sign + 1;
        } else {
            host = authority;
        }

        host_end = authority_end;
        if(host != NULL && host < authority_end && host[0] == '[') {
            const char* ipv6_end = strchr(host, ']');
            if(ipv6_end != NULL && ipv6_end < authority_end) {
                host_end = ipv6_end + 1;
                if(host_end < authority_end && *host_end == ':') {
                    port_sep = host_end;
                }
            }
        } else if(host != NULL) {
            for(const char* cursor = host; cursor < authority_end; cursor++) {
                if(*cursor == ':') {
                    port_sep = cursor;
                }
            }
            if(port_sep != NULL) {
                host_end = port_sep;
            }
        }

        if(port_sep != NULL && port_sep + 1 < authority_end) {
            port = port_sep + 1;
        }
    }

    path = (authority_end != NULL) ? authority_end : remainder;
    query = (path != NULL) ? strchr(path, '?') : NULL;

    if(flippass_output_token_n_equals(component, component_len, "RMVSCM")) {
        if(scheme_sep != NULL) {
            const char* stripped = scheme_sep + 1;
            if(stripped[0] == '/' && stripped[1] == '/') {
                stripped += 2;
            }
            furi_string_cat_str(out, stripped);
        } else {
            furi_string_cat_str(out, url);
        }
        return;
    }

    if(flippass_output_token_n_equals(component, component_len, "SCM")) {
        if(scheme_sep != NULL) {
            flippass_output_cat_strn(out, url, (size_t)(scheme_sep - url));
        }
        return;
    }

    if(flippass_output_token_n_equals(component, component_len, "HOST")) {
        if(host != NULL && host_end != NULL && host_end > host) {
            flippass_output_cat_strn(out, host, (size_t)(host_end - host));
        }
        return;
    }

    if(flippass_output_token_n_equals(component, component_len, "PORT")) {
        if(port != NULL && authority_end != NULL && authority_end > port) {
            flippass_output_cat_strn(out, port, (size_t)(authority_end - port));
        }
        return;
    }

    if(flippass_output_token_n_equals(component, component_len, "PATH")) {
        const char* path_end = path;
        if(path_end != NULL) {
            while(*path_end != '\0' && *path_end != '?' && *path_end != '#') {
                path_end++;
            }
            if(path_end > path) {
                flippass_output_cat_strn(out, path, (size_t)(path_end - path));
            }
        }
        return;
    }

    if(flippass_output_token_n_equals(component, component_len, "QUERY")) {
        if(query != NULL) {
            const char* query_end = query;
            while(*query_end != '\0' && *query_end != '#') {
                query_end++;
            }
            if(query_end > query) {
                flippass_output_cat_strn(out, query, (size_t)(query_end - query));
            }
        }
        return;
    }

    if(flippass_output_token_n_equals(component, component_len, "USERINFO")) {
        if(userinfo != NULL && at_sign != NULL && at_sign > userinfo) {
            flippass_output_cat_strn(out, userinfo, (size_t)(at_sign - userinfo));
        }
        return;
    }

    if(flippass_output_token_n_equals(component, component_len, "USERNAME")) {
        if(userinfo != NULL && at_sign != NULL && at_sign > userinfo) {
            const char* colon = strchr(userinfo, ':');
            const char* end = (colon != NULL && colon < at_sign) ? colon : at_sign;
            if(end > userinfo) {
                flippass_output_cat_strn(out, userinfo, (size_t)(end - userinfo));
            }
        }
        return;
    }

    if(flippass_output_token_n_equals(component, component_len, "PASSWORD")) {
        if(userinfo != NULL && at_sign != NULL && at_sign > userinfo) {
            const char* colon = strchr(userinfo, ':');
            if(colon != NULL && colon + 1 < at_sign) {
                flippass_output_cat_strn(out, colon + 1, (size_t)(at_sign - colon - 1));
            }
        }
    }
}

static bool flippass_output_append_placeholder_text(
    const FlipPassOutputActionRequestV1* request,
    const char* token,
    size_t token_len,
    FuriString* out,
    FlipPassOutputLiteralMode* mode) {
    furi_string_reset(out);
    if(mode != NULL) {
        *mode = FlipPassOutputLiteralModePlain;
    }

    if(flippass_output_token_n_equals(token, token_len, "USERNAME") ||
       flippass_output_token_n_equals(token, token_len, "S:USERNAME") ||
       flippass_output_token_n_equals(token, token_len, "S:UserName")) {
        if(request->entry_username != NULL) {
            furi_string_cat_str(out, request->entry_username);
        }
        if(mode != NULL) {
            *mode = FlipPassOutputLiteralModeAltNumpad;
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "PASSWORD") ||
       flippass_output_token_n_equals(token, token_len, "S:PASSWORD")) {
        if(request->entry_password != NULL) {
            furi_string_cat_str(out, request->entry_password);
        }
        if(mode != NULL) {
            *mode = FlipPassOutputLiteralModeAltNumpad;
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "URL") ||
       flippass_output_token_n_equals(token, token_len, "S:URL")) {
        if(request->entry_url != NULL) {
            furi_string_cat_str(out, request->entry_url);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "TITLE") ||
       flippass_output_token_n_equals(token, token_len, "S:TITLE") ||
       flippass_output_token_n_equals(token, token_len, "S:Title")) {
        if(request->entry_title != NULL) {
            furi_string_cat_str(out, request->entry_title);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "UUID")) {
        if(request->entry_uuid != NULL) {
            furi_string_cat_str(out, request->entry_uuid);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "NOTES") ||
       flippass_output_token_n_equals(token, token_len, "S:NOTES") ||
       flippass_output_token_n_equals(token, token_len, "S:Notes")) {
        if(request->entry_notes != NULL) {
            furi_string_cat_str(out, request->entry_notes);
        }
        return true;
    }

    if(flippass_output_token_n_starts_with(token, token_len, "URL:")) {
        if(request->entry_url != NULL) {
            flippass_output_append_url_component(
                out, request->entry_url, token + 4, token_len - 4U);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "GROUP")) {
        if(request->group_name != NULL) {
            furi_string_cat_str(out, request->group_name);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "GROUP_PATH") ||
       flippass_output_token_n_equals(token, token_len, "GROUPPATH")) {
        if(request->group_path != NULL) {
            furi_string_cat_str(out, request->group_path);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_PATH")) {
        if(request->db_path != NULL) {
            furi_string_set_str(out, request->db_path);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_DIR") ||
       flippass_output_token_n_equals(token, token_len, "DOCDIR")) {
        if(request->db_dir != NULL) {
            furi_string_set_str(out, request->db_dir);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_NAME")) {
        if(request->db_name != NULL) {
            furi_string_set_str(out, request->db_name);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_BASENAME")) {
        if(request->db_basename != NULL) {
            furi_string_set_str(out, request->db_basename);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_EXT")) {
        if(request->db_ext != NULL) {
            furi_string_set_str(out, request->db_ext);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "ENV_DIRSEP")) {
        furi_string_cat_str(out, "/");
        return true;
    }

    if(flippass_output_token_n_starts_with(token, token_len, "C:")) {
        return true;
    }

    return false;
}

static bool flippass_output_vk_to_hid(uint32_t vkey, bool force_extended, uint16_t* hid_key) {
    if(hid_key == NULL) {
        return false;
    }

    switch(vkey) {
    case 0x08:
        *hid_key = HID_KEYBOARD_DELETE;
        return true;
    case 0x09:
        *hid_key = HID_KEYBOARD_TAB;
        return true;
    case 0x0D:
        *hid_key = force_extended ? HID_KEYPAD_ENTER : HID_KEYBOARD_RETURN;
        return true;
    case 0x10:
        *hid_key = KEY_MOD_LEFT_SHIFT;
        return true;
    case 0x11:
        *hid_key = KEY_MOD_LEFT_CTRL;
        return true;
    case 0x12:
        *hid_key = KEY_MOD_LEFT_ALT;
        return true;
    case 0x13:
        *hid_key = HID_KEYBOARD_PAUSE;
        return true;
    case 0x14:
        *hid_key = HID_KEYBOARD_CAPS_LOCK;
        return true;
    case 0x1B:
        *hid_key = HID_KEYBOARD_ESCAPE;
        return true;
    case 0x20:
        *hid_key = HID_KEYBOARD_SPACEBAR;
        return true;
    case 0x21:
        *hid_key = HID_KEYBOARD_PAGE_UP;
        return true;
    case 0x22:
        *hid_key = HID_KEYBOARD_PAGE_DOWN;
        return true;
    case 0x23:
        *hid_key = HID_KEYBOARD_END;
        return true;
    case 0x24:
        *hid_key = HID_KEYBOARD_HOME;
        return true;
    case 0x25:
        *hid_key = HID_KEYBOARD_LEFT_ARROW;
        return true;
    case 0x26:
        *hid_key = HID_KEYBOARD_UP_ARROW;
        return true;
    case 0x27:
        *hid_key = HID_KEYBOARD_RIGHT_ARROW;
        return true;
    case 0x28:
        *hid_key = HID_KEYBOARD_DOWN_ARROW;
        return true;
    case 0x2C:
        *hid_key = HID_KEYBOARD_PRINT_SCREEN;
        return true;
    case 0x2D:
        *hid_key = HID_KEYBOARD_INSERT;
        return true;
    case 0x2E:
        *hid_key = HID_KEYBOARD_DELETE_FORWARD;
        return true;
    case 0x5B:
        *hid_key = KEY_MOD_LEFT_GUI;
        return true;
    case 0x5C:
        *hid_key = KEY_MOD_RIGHT_GUI;
        return true;
    case 0x5D:
        *hid_key = HID_KEYBOARD_APPLICATION;
        return true;
    case 0x6A:
        *hid_key = HID_KEYPAD_ASTERISK;
        return true;
    case 0x6B:
        *hid_key = HID_KEYPAD_PLUS;
        return true;
    case 0x6C:
        *hid_key = HID_KEYBOARD_SEPARATOR;
        return true;
    case 0x6D:
        *hid_key = HID_KEYPAD_MINUS;
        return true;
    case 0x6E:
        *hid_key = HID_KEYPAD_DOT;
        return true;
    case 0x6F:
        *hid_key = HID_KEYPAD_SLASH;
        return true;
    case 0x90:
        *hid_key = HID_KEYPAD_NUMLOCK;
        return true;
    case 0x91:
        *hid_key = HID_KEYBOARD_SCROLL_LOCK;
        return true;
    default:
        break;
    }

    if(vkey >= 0x30U && vkey <= 0x39U) {
        *hid_key = (vkey == 0x30U) ? HID_KEYBOARD_0 : (uint16_t)(HID_KEYBOARD_1 + vkey - 0x31U);
        return true;
    }

    if(vkey >= 0x41U && vkey <= 0x5AU) {
        *hid_key = (uint16_t)(HID_KEYBOARD_A + vkey - 0x41U);
        return true;
    }

    if(vkey >= 0x60U && vkey <= 0x69U) {
        *hid_key = (uint16_t)(HID_KEYPAD_0 + vkey - 0x60U);
        return true;
    }

    if(vkey >= 0x70U && vkey <= 0x87U) {
        *hid_key = (uint16_t)(HID_KEYBOARD_F1 + vkey - 0x70U);
        return true;
    }

    return false;
}

static bool flippass_output_execute_sequence(
    FlipPassOutputSession* session,
    const char* sequence,
    size_t* index,
    uint16_t group_modifiers,
    bool in_group);

static bool flippass_output_execute_vkey(
    FlipPassOutputSession* session,
    const char* params,
    size_t params_len,
    uint16_t group_modifiers,
    bool force_extended) {
    uint32_t vkey = 0U;
    uint16_t hid_key = HID_KEYBOARD_NONE;
    bool key_down = false;
    bool key_up = false;
    size_t cursor = 0U;

    if(params == NULL) {
        return false;
    }

    while(cursor < params_len && (params[cursor] == ' ' || params[cursor] == '\t')) {
        cursor++;
    }

    {
        const size_t number_start = cursor;
        while(cursor < params_len && params[cursor] >= '0' && params[cursor] <= '9') {
            cursor++;
        }
        if(number_start == cursor) {
            return false;
        }

        if(!flippass_output_parse_uint_n(params + number_start, cursor - number_start, &vkey)) {
            return false;
        }
    }

    while(cursor < params_len) {
        while(cursor < params_len && (params[cursor] == ' ' || params[cursor] == '\t')) {
            cursor++;
        }
        if(cursor >= params_len) {
            break;
        }

        const size_t flag_start = cursor;
        while(cursor < params_len && params[cursor] != ' ' && params[cursor] != '\t') {
            cursor++;
        }

        for(size_t flag = flag_start; flag < cursor; flag++) {
            switch(params[flag]) {
            case 'E':
            case 'e':
                force_extended = true;
                break;
            case 'D':
            case 'd':
                key_down = true;
                key_up = false;
                break;
            case 'U':
            case 'u':
                key_up = true;
                key_down = false;
                break;
            case 'N':
            case 'n':
                force_extended = false;
                break;
            default:
                return false;
            }
        }
    }

    if(!flippass_output_vk_to_hid(vkey, force_extended, &hid_key)) {
        return false;
    }

    if((hid_key & 0xFFU) == 0U) {
        if(key_down) {
            session->sticky_modifiers |= hid_key;
            return flippass_output_session_set_modifiers(session, group_modifiers);
        }
        if(key_up) {
            session->sticky_modifiers &= ~hid_key;
            return flippass_output_session_set_modifiers(session, group_modifiers);
        }
        return flippass_output_session_tap_modifier_only(session, hid_key);
    }

    if(key_down) {
        if(!flippass_output_session_press_prepared(session, hid_key)) {
            return false;
        }
        return flippass_output_session_inter_key_delay(session);
    }

    if(key_up) {
        if(!flippass_output_session_release_prepared(session, hid_key)) {
            return false;
        }
        return flippass_output_session_inter_key_delay(session);
    }

    return flippass_output_session_tap_key(session, hid_key);
}

static bool flippass_output_execute_braced_token(
    FlipPassOutputSession* session,
    const char* sequence,
    size_t* index,
    uint16_t group_modifiers) {
    size_t token_start = *index + 1U;
    size_t token_end = token_start;
    size_t name_end = 0U;
    size_t params_start = 0U;
    bool ok = false;

    while(sequence[token_start] == ' ' || sequence[token_start] == '\t') {
        token_start++;
    }

    while(sequence[token_end] != '\0' && sequence[token_end] != '}') {
        token_end++;
    }

    if(sequence[token_end] != '}' || token_end == token_start) {
        return false;
    }

    *index = token_end + 1U;

    name_end = token_start;
    while(name_end < token_end && sequence[name_end] != ' ' && sequence[name_end] != '\t') {
        name_end++;
    }

    params_start = name_end;
    while(params_start < token_end &&
          (sequence[params_start] == ' ' || sequence[params_start] == '\t')) {
        params_start++;
    }

    const char* name = sequence + token_start;
    const size_t name_len = name_end - token_start;
    const char* params = sequence + params_start;
    const size_t params_len = token_end - params_start;
    const bool no_params = (params_len == 0U);

    if(flippass_output_token_n_equals(name, name_len, "DELAY")) {
        uint32_t delay_ms = 250U;
        if(!no_params && !flippass_output_parse_uint_n(params, params_len, &delay_ms)) {
            return false;
        }
        return flippass_output_session_delay(session, delay_ms);
    }

    if(flippass_output_token_n_starts_with(name, name_len, "DELAY=")) {
        uint32_t delay_ms = 0U;
        if(!no_params || name_len <= 6U ||
           !flippass_output_parse_uint_n(name + 6, name_len - 6U, &delay_ms)) {
            return false;
        }
        session->default_delay_ms = delay_ms;
        return true;
    }

    if(flippass_output_token_n_equals(name, name_len, "VKEY") ||
       flippass_output_token_n_equals(name, name_len, "VKEY-EX") ||
       flippass_output_token_n_equals(name, name_len, "VKEY-NX")) {
        return flippass_output_execute_vkey(
            session,
            params,
            params_len,
            group_modifiers,
            flippass_output_token_n_equals(name, name_len, "VKEY-EX"));
    }

    if(flippass_output_token_n_equals(name, name_len, "CLEARFIELD") && no_params) {
        size_t nested_index = 0U;
        return flippass_output_execute_sequence(
            session, FLIPPASS_OUTPUT_CLEARFIELD_SEQUENCE, &nested_index, group_modifiers, false);
    }

    if(flippass_output_token_n_equals(name, name_len, "APPACTIVATE") ||
       flippass_output_token_n_equals(name, name_len, "BEEP")) {
        return true;
    }

    if(no_params && session->placeholder_buffer != NULL) {
        FlipPassOutputLiteralMode literal_mode = FlipPassOutputLiteralModePlain;
        const bool handled = flippass_output_append_placeholder_text(
            session->request, name, name_len, session->placeholder_buffer, &literal_mode);
        if(handled) {
            UNUSED(literal_mode);
            return flippass_output_session_type_literal(
                session, furi_string_get_cstr(session->placeholder_buffer));
        }
    }

    {
        uint32_t repeat = 1U;
        const FlipPassOutputSpecialKey* special =
            flippass_output_find_special_key_n(name, name_len);
        const bool has_repeat = !no_params &&
                                flippass_output_parse_uint_n(params, params_len, &repeat);

        if(special != NULL && (no_params || has_repeat)) {
            ok = true;
            for(uint32_t count = 0U; ok && count < repeat; count++) {
                ok = flippass_output_session_tap_key(session, special->hid_key);
            }
            return ok;
        }

        if(name_len == 1U && (no_params || has_repeat)) {
            ok = true;
            for(uint32_t count = 0U; ok && count < repeat; count++) {
                ok = flippass_output_session_tap_char(session, name[0]);
            }
            return ok;
        }
    }

    return false;
}

static bool flippass_output_execute_sequence(
    FlipPassOutputSession* session,
    const char* sequence,
    size_t* index,
    uint16_t group_modifiers,
    bool in_group) {
    uint16_t pending_modifiers = 0U;

    while(sequence[*index] != '\0') {
        const char ch = sequence[*index];
        const uint16_t symbol_modifier = flippass_output_modifier_from_symbol(ch);

        if(symbol_modifier != 0U) {
            if(!flippass_output_session_flush_pending_cr(session)) {
                return false;
            }
            pending_modifiers |= symbol_modifier;
            (*index)++;
            continue;
        }

        if(ch == '(') {
            if(!flippass_output_session_flush_pending_cr(session)) {
                return false;
            }
            (*index)++;
            if(!flippass_output_session_set_modifiers(
                   session, group_modifiers | pending_modifiers)) {
                return false;
            }
            if(!flippass_output_execute_sequence(
                   session, sequence, index, group_modifiers | pending_modifiers, true)) {
                return false;
            }
            if(!flippass_output_session_set_modifiers(session, group_modifiers)) {
                return false;
            }
            pending_modifiers = 0U;
            continue;
        }

        if(ch == ')') {
            if(!flippass_output_session_flush_pending_cr(session)) {
                return false;
            }
            if(!in_group) {
                return false;
            }
            (*index)++;
            return flippass_output_session_set_modifiers(session, group_modifiers);
        }

        if(!flippass_output_session_set_modifiers(session, group_modifiers | pending_modifiers)) {
            return false;
        }

        if(ch == '{') {
            if(!flippass_output_session_flush_pending_cr(session)) {
                return false;
            }
            if(!flippass_output_execute_braced_token(session, sequence, index, group_modifiers)) {
                return false;
            }
        } else if(ch == '}') {
            if(!flippass_output_session_flush_pending_cr(session)) {
                return false;
            }
            return false;
        } else if(ch == '~') {
            if(!flippass_output_session_flush_pending_cr(session)) {
                return false;
            }
            if(!flippass_output_session_tap_key(session, HID_KEYBOARD_RETURN)) {
                return false;
            }
            (*index)++;
        } else {
            if(!flippass_output_session_type_text_byte(session, (uint8_t)ch)) {
                return false;
            }
            (*index)++;
        }

        flippass_output_progress_update(session, *index);
        if(!flippass_output_session_set_modifiers(session, group_modifiers)) {
            return false;
        }
        pending_modifiers = 0U;
    }

    return !in_group && flippass_output_session_flush_pending_cr(session);
}

static FlipPassOutputSession flippass_output_session_make(
    const FlipPassOutputActionRequestV1* request,
    const FlipPassOutputActionHostApiV1* host_api) {
    FlipPassOutputSession session = {
        .request = request,
        .host_api = host_api,
        .transport = request->transport,
    };
    return session;
}

static bool flippass_output_execute_string_request(
    const FlipPassOutputActionRequestV1* request,
    const FlipPassOutputActionHostApiV1* host_api) {
    FlipPassOutputSession session = flippass_output_session_make(request, host_api);

    if(request->text == NULL) {
        return false;
    }

    if(!flippass_output_session_begin(&session)) {
        flippass_output_session_end(&session);
        return false;
    }

    flippass_output_progress_begin(&session, strlen(request->text), "Sending text.");
    const bool ok = flippass_output_session_type_literal(&session, request->text);
    flippass_output_session_end(&session);
    return ok;
}

static bool flippass_output_execute_login_request(
    const FlipPassOutputActionRequestV1* request,
    const FlipPassOutputActionHostApiV1* host_api) {
    FlipPassOutputSession session = flippass_output_session_make(request, host_api);
    bool ok = false;
    size_t completed_steps = 0U;

    if(request->username == NULL || request->password == NULL) {
        return false;
    }

    const size_t username_len = strlen(request->username);
    const size_t password_len = strlen(request->password);

    if(!flippass_output_session_begin(&session)) {
        flippass_output_session_end(&session);
        return false;
    }

    flippass_output_progress_begin(
        &session, username_len + password_len + 2U, "Sending sequence.");

    ok = flippass_output_session_stream_cstr(&session, request->username, &completed_steps);
    if(ok) {
        ok = flippass_output_session_tap_key(&session, HID_KEYBOARD_TAB);
        if(ok) {
            completed_steps++;
            flippass_output_progress_update(&session, completed_steps);
        }
    }
    if(ok) {
        ok = flippass_output_session_stream_cstr(&session, request->password, &completed_steps);
    }
    if(ok) {
        ok = flippass_output_session_tap_key(&session, HID_KEYBOARD_RETURN);
        if(ok) {
            completed_steps++;
            flippass_output_progress_update(&session, completed_steps);
        }
    }

    flippass_output_session_end(&session);
    return ok;
}

static bool flippass_output_execute_vault_ref_request(
    const FlipPassOutputActionRequestV1* request,
    const FlipPassOutputActionHostApiV1* host_api) {
    FlipPassOutputSession session = flippass_output_session_make(request, host_api);
    size_t completed_steps = 0U;

    if(request->primary_ref_plain_len == 0U) {
        return false;
    }

    if(!flippass_output_session_begin(&session)) {
        flippass_output_session_end(&session);
        return false;
    }

    flippass_output_progress_begin(&session, request->primary_ref_plain_len, "Sending text.");
    const bool ok = flippass_output_session_stream_vault_ref(
        &session,
        FlipPassOutputActionPluginRefPrimary,
        request->primary_ref_plain_len,
        &completed_steps);
    flippass_output_session_end(&session);
    return ok;
}

static bool flippass_output_execute_login_refs_request(
    const FlipPassOutputActionRequestV1* request,
    const FlipPassOutputActionHostApiV1* host_api) {
    FlipPassOutputSession session = flippass_output_session_make(request, host_api);
    size_t completed_steps = 0U;
    bool ok = false;
    const size_t total_steps =
        request->username_ref_plain_len + request->password_ref_plain_len + 2U;

    if(request->username_ref_plain_len == 0U || request->password_ref_plain_len == 0U) {
        return false;
    }

    if(!flippass_output_session_begin(&session)) {
        flippass_output_session_end(&session);
        return false;
    }

    flippass_output_progress_begin(&session, total_steps, "Sending sequence.");

    ok = flippass_output_session_stream_vault_ref(
        &session,
        FlipPassOutputActionPluginRefUsername,
        request->username_ref_plain_len,
        &completed_steps);
    if(ok) {
        ok = flippass_output_session_tap_key(&session, HID_KEYBOARD_TAB);
        if(ok) {
            completed_steps++;
            flippass_output_progress_update(&session, completed_steps);
        }
    }
    if(ok) {
        ok = flippass_output_session_stream_vault_ref(
            &session,
            FlipPassOutputActionPluginRefPassword,
            request->password_ref_plain_len,
            &completed_steps);
    }
    if(ok) {
        ok = flippass_output_session_tap_key(&session, HID_KEYBOARD_RETURN);
        if(ok) {
            completed_steps++;
            flippass_output_progress_update(&session, completed_steps);
        }
    }

    flippass_output_session_end(&session);
    return ok;
}

static bool flippass_output_execute_autotype_request(
    const FlipPassOutputActionRequestV1* request,
    const FlipPassOutputActionHostApiV1* host_api) {
    const char* sequence =
        (request->autotype_sequence != NULL && request->autotype_sequence[0] != '\0') ?
            request->autotype_sequence :
            FLIPPASS_OUTPUT_KEEPASS_DEFAULT_SEQUENCE;
    FlipPassOutputSession session = flippass_output_session_make(request, host_api);
    size_t index = 0U;

    session.placeholder_buffer = furi_string_alloc();
    if(session.placeholder_buffer == NULL) {
        return false;
    }
    furi_string_reserve(session.placeholder_buffer, 128U);

    if(!flippass_output_session_begin(&session)) {
        flippass_output_session_end(&session);
        furi_string_free(session.placeholder_buffer);
        return false;
    }

    flippass_output_progress_begin(&session, strlen(sequence), "Sending sequence.");
    const bool ok = flippass_output_execute_sequence(&session, sequence, &index, 0U, false);

    flippass_output_session_end(&session);
    furi_string_free(session.placeholder_buffer);
    return ok;
}

static bool flippass_output_action_run(
    const FlipPassOutputActionRequestV1* request,
    const FlipPassOutputActionHostApiV1* host_api,
    FuriString* error) {
    UNUSED(error);

    if(request == NULL || host_api == NULL ||
       request->api_version != FLIPPASS_OUTPUT_ACTION_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_OUTPUT_ACTION_HOST_API_VERSION) {
        return false;
    }

    if(host_api->should_cancel != NULL && host_api->should_cancel(host_api->context)) {
        return false;
    }

    switch(request->action) {
    case FlipPassOutputActionPluginKindString:
        return flippass_output_execute_string_request(request, host_api);
    case FlipPassOutputActionPluginKindLogin:
        return flippass_output_execute_login_request(request, host_api);
    case FlipPassOutputActionPluginKindVaultRef:
        return flippass_output_execute_vault_ref_request(request, host_api);
    case FlipPassOutputActionPluginKindLoginRefs:
        return flippass_output_execute_login_refs_request(request, host_api);
    case FlipPassOutputActionPluginKindAutotype:
        return flippass_output_execute_autotype_request(request, host_api);
    default:
        return false;
    }
}

static const FlipPassOutputActionPluginV1 flippass_output_action_plugin = {
    .api_version = FLIPPASS_OUTPUT_ACTION_PLUGIN_API_VERSION,
    .run = flippass_output_action_run,
};

static const FlipperAppPluginDescriptor flippass_output_action_descriptor = {
    .appid = FLIPPASS_OUTPUT_ACTION_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OUTPUT_ACTION_PLUGIN_API_VERSION,
    .entry_point = &flippass_output_action_plugin,
};

const FlipperAppPluginDescriptor* flippass_output_action_plugin_ep(void) {
    return &flippass_output_action_descriptor;
}
