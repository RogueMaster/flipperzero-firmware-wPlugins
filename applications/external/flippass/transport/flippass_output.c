#include "../flippass.h"
#include "../flippass_db.h"

#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include <gap.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_BT_KEYS_STORAGE_DIR    EXT_PATH("apps_data/bad_usb")
#define FLIPPASS_BT_KEYS_STORAGE_PATH   EXT_PATH("apps_data/bad_usb/.bt_hid.keys")
#define FLIPPASS_BLE_SETUP_DELAY_MS     200U
#define FLIPPASS_BLE_WAIT_STEP_MS       100U
#define FLIPPASS_BLE_CONNECT_TIMEOUT_MS 15000U
#define FLIPPASS_BLE_CONNECT_SETTLE_MS  300U
#define FLIPPASS_BLE_PRESS_DELAY_MS     12U
#define FLIPPASS_BLE_RELEASE_DELAY_MS   18U
#define FLIPPASS_BLE_STEP_DELAY_MS      45U
#define FLIPPASS_BLE_NAME_PREFIX        "BadUSB"
#define FLIPPASS_BLE_NAME_BUFFER_SIZE   21U
#define FLIPPASS_BLE_MAC_XOR            0x0002U
#define FLIPPASS_OUTPUT_KEEPASS_DEFAULT_SEQUENCE "{USERNAME}{TAB}{PASSWORD}{ENTER}"
#define FLIPPASS_OUTPUT_CLEARFIELD_SEQUENCE      "{HOME}+({END}){BKSP}{DELAY 50}"

typedef struct {
    char name[FLIPPASS_BLE_NAME_BUFFER_SIZE];
    uint8_t mac[GAP_MAC_ADDR_SIZE];
    bool bonding;
    GapPairing pairing;
} FlipPassBleProfileParams;

struct FlipPassBleSession {
    Bt* bt;
    FuriHalBleProfileBase* profile;
    bool connected;
    BtStatus status;
};

typedef struct {
    const char* token;
    uint16_t hid_key;
} FlipPassOutputSpecialKey;

typedef struct {
    App* app;
    FlipPassOutputTransport transport;
    uint16_t sticky_modifiers;
    uint16_t current_modifiers;
    uint32_t default_delay_ms;
    size_t progress_total;
    const char* progress_detail;
    FuriString* placeholder_buffer;
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

static void flippass_output_progress_begin(
    FlipPassOutputSession* session,
    size_t total_steps,
    const char* detail) {
    if(session == NULL || session->app == NULL) {
        return;
    }

    session->progress_total = (total_steps > 0U) ? total_steps : 1U;
    session->progress_detail = detail;
    flippass_progress_update(session->app, "Typing", detail, 45U);
}

static void flippass_output_progress_update(
    FlipPassOutputSession* session,
    size_t completed_steps) {
    uint8_t percent = 45U;

    if(session == NULL || session->app == NULL || session->progress_total == 0U) {
        return;
    }

    if(completed_steps > session->progress_total) {
        completed_steps = session->progress_total;
    }

    percent = (uint8_t)(45U + ((completed_steps * 53U) / session->progress_total));
    if(percent > 98U) {
        percent = 98U;
    }
    if(percent <= session->app->progress_percent && completed_steps < session->progress_total) {
        return;
    }

    flippass_progress_update(session->app, "Typing", session->progress_detail, percent);
}

static void flippass_ble_release_all(App* app) {
    if(app == NULL || app->ble_session == NULL || app->ble_session->profile == NULL) {
        return;
    }

    ble_profile_hid_kb_release_all(app->ble_session->profile);
}

static bool flippass_ble_press_key_prepared(App* app, uint16_t hid_key) {
    furi_assert(app);

    if(hid_key == HID_KEYBOARD_NONE || app->ble_session == NULL || app->ble_session->profile == NULL) {
        return false;
    }

    ble_profile_hid_kb_press(app->ble_session->profile, hid_key);
    return true;
}

static bool flippass_ble_release_key_prepared(App* app, uint16_t hid_key) {
    furi_assert(app);

    if(hid_key == HID_KEYBOARD_NONE || app->ble_session == NULL || app->ble_session->profile == NULL) {
        return false;
    }

    ble_profile_hid_kb_release(app->ble_session->profile, hid_key);
    return true;
}

void flippass_output_bluetooth_get_name(char* buffer, size_t size) {
    if(buffer == NULL || size == 0U) {
        return;
    }

    snprintf(buffer, size, "%s %s", FLIPPASS_BLE_NAME_PREFIX, furi_hal_version_get_name_ptr());
}

static FuriHalBleProfileBase* flippass_ble_profile_start(FuriHalBleProfileParams profile_params) {
    UNUSED(profile_params);

    return ble_profile_hid->start(NULL);
}

static void flippass_ble_profile_stop(FuriHalBleProfileBase* profile) {
    ble_profile_hid->stop(profile);
}

static void
    flippass_ble_profile_get_config(GapConfig* config, FuriHalBleProfileParams profile_params) {
    furi_check(config);
    FlipPassBleProfileParams* params = profile_params;

    /* Reuse the stock HID BLE identity so hosts already paired with BadUSB reconnect here. */
    ble_profile_hid->get_gap_config(config, NULL);

    if(params != NULL) {
        memcpy(config->mac_address, params->mac, sizeof(config->mac_address));
        strlcpy(config->adv_name + 1, params->name, sizeof(config->adv_name) - 1);
        config->bonding_mode = params->bonding;
        config->pairing_method = params->pairing;
    }
}

static const FuriHalBleProfileTemplate flippass_ble_profile_template = {
    .start = flippass_ble_profile_start,
    .stop = flippass_ble_profile_stop,
    .get_gap_config = flippass_ble_profile_get_config,
};

static void flippass_ble_status_changed(BtStatus status, void* context) {
    FlipPassBleSession* session = context;
    furi_assert(session);
    session->status = status;
    session->connected = (status == BtStatusConnected);
}

static bool flippass_ble_prepare_storage(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool ok = storage_simply_mkdir(storage, FLIPPASS_BT_KEYS_STORAGE_DIR);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void flippass_ble_prepare_profile_params(FlipPassBleProfileParams* profile_params) {
    furi_assert(profile_params);

    memset(profile_params, 0, sizeof(*profile_params));
    memcpy(profile_params->mac, furi_hal_version_get_ble_mac(), sizeof(profile_params->mac));
    profile_params->mac[2]++;
    profile_params->mac[0] ^= FLIPPASS_BLE_MAC_XOR;
    profile_params->mac[1] ^= FLIPPASS_BLE_MAC_XOR >> 8;
    profile_params->bonding = true;
    profile_params->pairing = GapPairingPinCodeVerifyYesNo;
    flippass_output_bluetooth_get_name(profile_params->name, sizeof(profile_params->name));
}

static bool flippass_ble_session_start(App* app) {
    furi_assert(app);

    if(app->ble_session != NULL) {
        return true;
    }

    if(!flippass_ble_prepare_storage()) {
        return false;
    }

    FlipPassBleSession* session = malloc(sizeof(FlipPassBleSession));
    if(session == NULL) {
        return false;
    }
    memset(session, 0, sizeof(*session));

    session->bt = furi_record_open(RECORD_BT);
    bt_disconnect(session->bt);
    furi_delay_ms(FLIPPASS_BLE_SETUP_DELAY_MS);
    bt_keys_storage_set_storage_path(session->bt, FLIPPASS_BT_KEYS_STORAGE_PATH);

    FlipPassBleProfileParams profile_params;
    flippass_ble_prepare_profile_params(&profile_params);

    session->profile = bt_profile_start(
        session->bt, &flippass_ble_profile_template, (void*)&profile_params);
    if(session->profile == NULL) {
        bt_keys_storage_set_default_path(session->bt);
        furi_record_close(RECORD_BT);
        free(session);
        return false;
    }

    session->connected = false;
    session->status = BtStatusOff;
    bt_set_status_changed_callback(session->bt, flippass_ble_status_changed, session);
    furi_hal_bt_start_advertising();
    session->status = BtStatusAdvertising;

    app->ble_session = session;
    return true;
}

static bool flippass_ble_ensure_advertising(App* app) {
    furi_assert(app);

    if(!flippass_ble_session_start(app)) {
        return false;
    }

    FlipPassBleSession* session = app->ble_session;
    furi_assert(session);

    if(!session->connected) {
        flippass_ble_release_all(app);
        bt_disconnect(session->bt);
        furi_delay_ms(FLIPPASS_BLE_SETUP_DELAY_MS);
        furi_hal_bt_start_advertising();
        session->status = BtStatusAdvertising;
    }

    return true;
}

bool flippass_output_bluetooth_is_connected(const App* app) {
    return app != NULL && app->ble_session != NULL && app->ble_session->connected;
}

bool flippass_output_bluetooth_is_advertising(const App* app) {
    return app != NULL && app->ble_session != NULL && !app->ble_session->connected &&
           app->ble_session->status == BtStatusAdvertising;
}

bool flippass_output_bluetooth_advertise(App* app) {
    furi_assert(app);
    return flippass_ble_ensure_advertising(app);
}

static bool flippass_ble_wait_connected(App* app) {
    furi_assert(app);

    if(!flippass_ble_ensure_advertising(app)) {
        return false;
    }

    uint32_t waited_ms = 0U;
    while(app->ble_session != NULL && !app->ble_session->connected &&
          waited_ms < FLIPPASS_BLE_CONNECT_TIMEOUT_MS) {
        furi_delay_ms(FLIPPASS_BLE_WAIT_STEP_MS);
        waited_ms += FLIPPASS_BLE_WAIT_STEP_MS;
        flippass_progress_update(
            app,
            "Connecting",
            "Waiting for Bluetooth HID host.",
            (uint8_t)(5U + ((waited_ms * 35U) / FLIPPASS_BLE_CONNECT_TIMEOUT_MS)));
    }

    if(app->ble_session == NULL || !app->ble_session->connected) {
        return false;
    }

    flippass_progress_update(app, "Typing", "Bluetooth HID connected.", 40U);
    furi_delay_ms(FLIPPASS_BLE_CONNECT_SETTLE_MS);
    return true;
}

static char flippass_output_ascii_upper(char value) {
    if(value >= 'a' && value <= 'z') {
        value = (char)(value - 'a' + 'A');
    }

    return value;
}

static bool flippass_output_token_n_equals(const char* token, size_t token_len, const char* expected) {
    if(token == NULL || expected == NULL) {
        return false;
    }

    size_t index = 0U;
    while(index < token_len && expected[index] != '\0') {
        if(flippass_output_ascii_upper(token[index]) != flippass_output_ascii_upper(expected[index])) {
            return false;
        }
        index++;
    }

    return (index == token_len) && (expected[index] == '\0');
}

static bool flippass_output_token_n_starts_with(
    const char* token,
    size_t token_len,
    const char* prefix) {
    if(token == NULL || prefix == NULL) {
        return false;
    }

    size_t index = 0U;
    while(prefix[index] != '\0') {
        if(index >= token_len) {
            return false;
        }

        if(flippass_output_ascii_upper(token[index]) != flippass_output_ascii_upper(prefix[index])) {
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

static bool flippass_output_session_press_prepared(
    FlipPassOutputSession* session,
    uint16_t hid_key) {
    furi_assert(session);

    switch(session->transport) {
    case FlipPassOutputTransportUsb:
        return flippass_usb_press_key_prepared(hid_key);
    case FlipPassOutputTransportBluetooth:
        return flippass_ble_press_key_prepared(session->app, hid_key);
    default:
        return false;
    }
}

static bool flippass_output_session_release_prepared(
    FlipPassOutputSession* session,
    uint16_t hid_key) {
    furi_assert(session);

    switch(session->transport) {
    case FlipPassOutputTransportUsb:
        return flippass_usb_release_key_prepared(hid_key);
    case FlipPassOutputTransportBluetooth:
        return flippass_ble_release_key_prepared(session->app, hid_key);
    default:
        return false;
    }
}

static void flippass_output_session_release_all_prepared(FlipPassOutputSession* session) {
    furi_assert(session);

    switch(session->transport) {
    case FlipPassOutputTransportUsb:
        flippass_usb_release_all_prepared();
        break;
    case FlipPassOutputTransportBluetooth:
        flippass_ble_release_all(session->app);
        break;
    default:
        break;
    }
}

static bool flippass_output_session_begin(FlipPassOutputSession* session) {
    furi_assert(session);

    session->sticky_modifiers = 0U;
    session->current_modifiers = 0U;
    session->default_delay_ms = (session->transport == FlipPassOutputTransportBluetooth) ?
                                    FLIPPASS_BLE_STEP_DELAY_MS :
                                    FLIPPASS_USB_STEP_DELAY_MS;

    switch(session->transport) {
    case FlipPassOutputTransportUsb:
        return flippass_usb_begin(session->app);
    case FlipPassOutputTransportBluetooth:
        if(!flippass_ble_wait_connected(session->app)) {
            return false;
        }
        flippass_ble_release_all(session->app);
        return true;
    default:
        return false;
    }
}

static void flippass_output_session_end(FlipPassOutputSession* session) {
    furi_assert(session);

    flippass_output_session_release_all_prepared(session);
    session->sticky_modifiers = 0U;
    session->current_modifiers = 0U;

    if(session->transport == FlipPassOutputTransportUsb) {
        flippass_usb_restore(session->app);
    }
}

static void flippass_output_session_inter_key_delay(FlipPassOutputSession* session) {
    furi_assert(session);

    if(session->default_delay_ms > 0U) {
        furi_delay_ms(session->default_delay_ms);
    }
}

static void flippass_output_session_pre_press_delay(uint32_t delay_ms) {
    if(delay_ms > 0U) {
        furi_delay_ms(delay_ms);
    }
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
        flippass_output_session_pre_press_delay(pre_press_delay);
        if(!flippass_output_session_press_prepared(session, modifier)) {
            return false;
        }
        session->current_modifiers |= modifier;
        flippass_output_session_inter_key_delay(session);
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
        flippass_output_session_inter_key_delay(session);
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
    const uint32_t press_delay = (session->transport == FlipPassOutputTransportBluetooth) ?
                                     FLIPPASS_BLE_PRESS_DELAY_MS :
                                     FLIPPASS_USB_PRESS_DELAY_MS;
    const uint32_t release_delay = (session->transport == FlipPassOutputTransportBluetooth) ?
                                       FLIPPASS_BLE_RELEASE_DELAY_MS :
                                       FLIPPASS_USB_RELEASE_DELAY_MS;

    if(base_key == HID_KEYBOARD_NONE) {
        return false;
    }

    if(temp_modifiers != 0U &&
       !flippass_output_session_press_modifier_mask(session, temp_modifiers)) {
        return false;
    }

    flippass_output_session_pre_press_delay(FLIPPASS_OUTPUT_PRE_PRESS_DELAY_MS);
    if(!flippass_output_session_press_prepared(session, base_key)) {
        if(temp_modifiers != 0U) {
            flippass_output_session_release_modifier_mask(session, temp_modifiers);
        }
        return false;
    }

    furi_delay_ms(press_delay);

    if(!flippass_output_session_release_prepared(session, base_key)) {
        flippass_output_session_release_all_prepared(session);
        return false;
    }

    furi_delay_ms(release_delay);

    if(temp_modifiers != 0U &&
       !flippass_output_session_release_modifier_mask(session, temp_modifiers)) {
        return false;
    }

    flippass_output_session_inter_key_delay(session);
    if(base_key == HID_KEYBOARD_TAB && session->default_delay_ms < 100U) {
        furi_delay_ms(session->default_delay_ms);
    }

    return true;
}

static bool flippass_output_session_tap_key(
    FlipPassOutputSession* session,
    uint16_t hid_key) {
    return flippass_output_session_tap_raw_key(
        session, hid_key & 0xFFU, hid_key & 0xFF00U);
}

static bool flippass_output_session_tap_char(FlipPassOutputSession* session, char ch) {
    const uint16_t hid_key = HID_ASCII_TO_KEY(ch);
    if(hid_key == HID_KEYBOARD_NONE) {
        return false;
    }

    return flippass_output_session_tap_key(session, hid_key);
}

static bool flippass_output_session_type_literal(
    FlipPassOutputSession* session,
    const char* text) {
    for(size_t index = 0U; text[index] != '\0'; index++) {
        if(!flippass_output_session_tap_char(session, text[index])) {
            return false;
        }
        flippass_output_progress_update(session, index + 1U);
    }

    return true;
}

static uint16_t flippass_output_numpad_key_from_digit(char digit) {
    return (digit >= '0' && digit <= '9') ? flippass_output_numpad_keys[digit - '0'] :
                                            HID_KEYBOARD_NONE;
}

static bool flippass_output_session_prepare_alt_numpad(FlipPassOutputSession* session) {
    if(session->transport != FlipPassOutputTransportUsb) {
        return true;
    }

    if((furi_hal_hid_get_led_state() & HID_KB_LED_NUM) != 0U) {
        return true;
    }

    return flippass_output_session_tap_raw_key(session, HID_KEYBOARD_LOCK_NUM_LOCK, 0U);
}

static bool flippass_output_session_type_alt_code_byte(
    FlipPassOutputSession* session,
    uint8_t value) {
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

static bool flippass_output_session_type_alt_code_text(
    FlipPassOutputSession* session,
    const char* text) {
    if(text == NULL) {
        return false;
    }

    if(!flippass_output_session_prepare_alt_numpad(session)) {
        return false;
    }

    for(size_t index = 0U; text[index] != '\0'; index++) {
        if(!flippass_output_session_type_alt_code_byte(session, (uint8_t)text[index])) {
            return false;
        }
        flippass_output_progress_update(session, index + 1U);
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

static const KDBXGroup* flippass_output_current_group(const App* app) {
    if(app == NULL) {
        return NULL;
    }

    if(app->current_group != NULL) {
        return app->current_group;
    }

    return app->active_group;
}

static void flippass_output_append_group_path(FuriString* out, const KDBXGroup* group) {
    if(out == NULL || group == NULL) {
        return;
    }

    if(group->parent != NULL && group->parent->parent != NULL) {
        flippass_output_append_group_path(out, group->parent);
        if(!furi_string_empty(out)) {
            furi_string_cat_str(out, ".");
        }
    }

    if(group->name != NULL) {
        furi_string_cat_str(out, group->name);
    }
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
    App* app,
    const KDBXEntry* entry,
    const char* token,
    size_t token_len,
    FuriString* out,
    FlipPassOutputLiteralMode* mode) {
    KDBXEntry* mutable_entry = (KDBXEntry*)entry;

    furi_string_reset(out);
    if(mode != NULL) {
        *mode = FlipPassOutputLiteralModePlain;
    }

    if(flippass_output_token_n_equals(token, token_len, "USERNAME") ||
       flippass_output_token_n_equals(token, token_len, "S:USERNAME") ||
       flippass_output_token_n_equals(token, token_len, "S:UserName")) {
        if(entry->username != NULL) {
            furi_string_cat_str(out, entry->username);
        }
        if(mode != NULL) {
            *mode = FlipPassOutputLiteralModeAltNumpad;
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "PASSWORD") ||
       flippass_output_token_n_equals(token, token_len, "S:PASSWORD")) {
        if(entry->password != NULL) {
            furi_string_cat_str(out, entry->password);
        }
        if(mode != NULL) {
            *mode = FlipPassOutputLiteralModeAltNumpad;
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "URL") ||
       flippass_output_token_n_equals(token, token_len, "S:URL")) {
        if(entry->url != NULL) {
            furi_string_cat_str(out, entry->url);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "TITLE") ||
       flippass_output_token_n_equals(token, token_len, "S:TITLE") ||
       flippass_output_token_n_equals(token, token_len, "S:Title")) {
        if(entry->title != NULL) {
            furi_string_cat_str(out, entry->title);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "UUID")) {
        if(entry->uuid != NULL) {
            furi_string_cat_str(out, entry->uuid);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "NOTES") ||
       flippass_output_token_n_equals(token, token_len, "S:NOTES") ||
       flippass_output_token_n_equals(token, token_len, "S:Notes")) {
        if(entry->notes == NULL && flippass_db_entry_has_field(entry, KDBXEntryFieldNotes) &&
           !flippass_db_ensure_entry_field(app, mutable_entry, KDBXEntryFieldNotes, NULL)) {
            return false;
        }
        if(entry->notes != NULL) {
            furi_string_cat_str(out, entry->notes);
        }
        return true;
    }

    if(flippass_output_token_n_starts_with(token, token_len, "URL:")) {
        if(entry->url != NULL) {
            flippass_output_append_url_component(out, entry->url, token + 4, token_len - 4U);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "GROUP")) {
        const KDBXGroup* group = flippass_output_current_group(app);
        if(group != NULL && group->name != NULL) {
            furi_string_cat_str(out, group->name);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "GROUP_PATH") ||
       flippass_output_token_n_equals(token, token_len, "GROUPPATH")) {
        flippass_output_append_group_path(out, flippass_output_current_group(app));
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_PATH")) {
        if(app->file_path != NULL) {
            furi_string_set(out, app->file_path);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_DIR") ||
       flippass_output_token_n_equals(token, token_len, "DOCDIR")) {
        if(app->file_path != NULL) {
            path_extract_dirname(furi_string_get_cstr(app->file_path), out);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_NAME")) {
        if(app->file_path != NULL) {
            path_extract_basename(furi_string_get_cstr(app->file_path), out);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_BASENAME")) {
        if(app->file_path != NULL) {
            path_extract_filename_no_ext(furi_string_get_cstr(app->file_path), out);
        }
        return true;
    }

    if(flippass_output_token_n_equals(token, token_len, "DB_EXT")) {
        if(app->file_path != NULL) {
            const char* path = furi_string_get_cstr(app->file_path);
            const char* slash = strrchr(path, '/');
            const char* dot = strrchr(path, '.');
            if(dot != NULL && (slash == NULL || dot > slash) && dot[1] != '\0') {
                furi_string_cat_str(out, dot + 1);
            }
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

static bool flippass_output_vk_to_hid(
    uint32_t vkey,
    bool force_extended,
    uint16_t* hid_key) {
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
    const KDBXEntry* entry,
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
        flippass_output_session_inter_key_delay(session);
        return true;
    }

    if(key_up) {
        if(!flippass_output_session_release_prepared(session, hid_key)) {
            return false;
        }
        flippass_output_session_inter_key_delay(session);
        return true;
    }

    return flippass_output_session_tap_key(session, hid_key);
}

static bool flippass_output_execute_braced_token(
    FlipPassOutputSession* session,
    const KDBXEntry* entry,
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
        furi_delay_ms(delay_ms);
        return true;
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
            session,
            entry,
            FLIPPASS_OUTPUT_CLEARFIELD_SEQUENCE,
            &nested_index,
            group_modifiers,
            false);
    }

    if(flippass_output_token_n_equals(name, name_len, "APPACTIVATE") ||
       flippass_output_token_n_equals(name, name_len, "BEEP")) {
        return true;
    }

    if(no_params && session->placeholder_buffer != NULL) {
        FlipPassOutputLiteralMode literal_mode = FlipPassOutputLiteralModePlain;
        const bool handled = flippass_output_append_placeholder_text(
            session->app, entry, name, name_len, session->placeholder_buffer, &literal_mode);
        if(handled) {
            return literal_mode == FlipPassOutputLiteralModeAltNumpad ?
                       flippass_output_session_type_alt_code_text(
                           session, furi_string_get_cstr(session->placeholder_buffer)) :
                       flippass_output_session_type_literal(
                           session, furi_string_get_cstr(session->placeholder_buffer));
        }
    }

    {
        uint32_t repeat = 1U;
        const FlipPassOutputSpecialKey* special = flippass_output_find_special_key_n(name, name_len);
        const bool has_repeat = !no_params && flippass_output_parse_uint_n(params, params_len, &repeat);

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
    const KDBXEntry* entry,
    const char* sequence,
    size_t* index,
    uint16_t group_modifiers,
    bool in_group) {
    uint16_t pending_modifiers = 0U;

    while(sequence[*index] != '\0') {
        const char ch = sequence[*index];
        const uint16_t symbol_modifier = flippass_output_modifier_from_symbol(ch);

        if(symbol_modifier != 0U) {
            pending_modifiers |= symbol_modifier;
            (*index)++;
            continue;
        }

        if(ch == '(') {
            (*index)++;
            if(!flippass_output_session_set_modifiers(session, group_modifiers | pending_modifiers)) {
                return false;
            }
            if(!flippass_output_execute_sequence(
                   session, entry, sequence, index, group_modifiers | pending_modifiers, true)) {
                return false;
            }
            if(!flippass_output_session_set_modifiers(session, group_modifiers)) {
                return false;
            }
            pending_modifiers = 0U;
            continue;
        }

        if(ch == ')') {
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
            if(!flippass_output_execute_braced_token(session, entry, sequence, index, group_modifiers)) {
                return false;
            }
        } else if(ch == '}') {
            return false;
        } else if(ch == '~') {
            if(!flippass_output_session_tap_key(session, HID_KEYBOARD_RETURN)) {
                return false;
            }
            (*index)++;
        } else {
            if(!flippass_output_session_tap_char(session, ch)) {
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

    return !in_group;
}

const char* flippass_output_transport_name(FlipPassOutputTransport transport) {
    switch(transport) {
    case FlipPassOutputTransportBluetooth:
        return "Bluetooth HID";
    case FlipPassOutputTransportUsb:
    default:
        return "USB HID";
    }
}

bool flippass_output_type_string(App* app, FlipPassOutputTransport transport, const char* text) {
    FlipPassOutputSession session = {
        .app = app,
        .transport = transport,
    };

    furi_assert(app);
    furi_assert(text);

    if(!flippass_output_session_begin(&session)) {
        flippass_output_session_end(&session);
        return false;
    }

    flippass_output_progress_begin(&session, strlen(text), "Sending selected text.");
    const bool ok = flippass_output_session_type_alt_code_text(&session, text);
    flippass_output_session_end(&session);
    return ok;
}

bool flippass_output_type_login(
    App* app,
    FlipPassOutputTransport transport,
    const char* username,
    const char* password) {
    FlipPassOutputSession session = {
        .app = app,
        .transport = transport,
    };
    bool ok = false;
    const size_t username_len = strlen(username);
    const size_t password_len = strlen(password);

    furi_assert(app);
    furi_assert(username);
    furi_assert(password);

    if(!flippass_output_session_begin(&session)) {
        flippass_output_session_end(&session);
        return false;
    }

    flippass_output_progress_begin(
        &session,
        username_len + password_len + 2U,
        "Sending login sequence.");

    ok = flippass_output_session_type_alt_code_text(&session, username);
    if(ok) {
        ok = flippass_output_session_tap_key(&session, HID_KEYBOARD_TAB);
        if(ok) {
            flippass_output_progress_update(&session, username_len + 1U);
        }
    }
    if(ok) {
        ok = flippass_output_session_type_alt_code_text(&session, password);
    }
    if(ok) {
        ok = flippass_output_session_tap_key(&session, HID_KEYBOARD_RETURN);
        if(ok) {
            flippass_output_progress_update(&session, username_len + password_len + 2U);
        }
    }

    flippass_output_session_end(&session);
    return ok;
}

bool flippass_output_type_autotype(
    App* app,
    FlipPassOutputTransport transport,
    const KDBXEntry* entry) {
    const char* sequence = NULL;
    FlipPassOutputSession session = {
        .app = app,
        .transport = transport,
    };
    size_t index = 0U;

    furi_assert(app);
    furi_assert(entry);

    KDBXEntry* mutable_entry = (KDBXEntry*)entry;

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldUsername) && entry->username == NULL) {
        if(!flippass_db_ensure_entry_field(app, mutable_entry, KDBXEntryFieldUsername, NULL)) {
            return false;
        }
    }
    if(flippass_db_entry_has_field(entry, KDBXEntryFieldPassword) && entry->password == NULL) {
        if(!flippass_db_ensure_entry_field(app, mutable_entry, KDBXEntryFieldPassword, NULL)) {
            return false;
        }
    }
    if(flippass_db_entry_has_field(entry, KDBXEntryFieldUrl) && entry->url == NULL) {
        if(!flippass_db_ensure_entry_field(app, mutable_entry, KDBXEntryFieldUrl, NULL)) {
            return false;
        }
    }
    if(flippass_db_entry_has_field(entry, KDBXEntryFieldAutotype) &&
       entry->autotype_sequence == NULL) {
        if(!flippass_db_ensure_entry_field(app, mutable_entry, KDBXEntryFieldAutotype, NULL)) {
            return false;
        }
    }

    sequence = (entry->autotype_sequence != NULL && entry->autotype_sequence[0] != '\0') ?
                   entry->autotype_sequence :
                   FLIPPASS_OUTPUT_KEEPASS_DEFAULT_SEQUENCE;

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

    flippass_output_progress_begin(&session, strlen(sequence), "Sending AutoType sequence.");
    const bool ok = flippass_output_execute_sequence(
        &session, entry, sequence, &index, 0U, false);

    flippass_output_session_end(&session);
    furi_string_free(session.placeholder_buffer);
    return ok;
}

void flippass_output_release_all(App* app) {
    furi_assert(app);

    flippass_usb_release_all_prepared();
    flippass_ble_release_all(app);
}

void flippass_output_cleanup(App* app) {
    furi_assert(app);

    flippass_usb_restore(app);

    if(app->ble_session == NULL) {
        return;
    }

    bt_set_status_changed_callback(app->ble_session->bt, NULL, NULL);
    flippass_ble_release_all(app);
    bt_disconnect(app->ble_session->bt);
    furi_delay_ms(FLIPPASS_BLE_SETUP_DELAY_MS);
    furi_hal_bt_stop_advertising();
    bt_keys_storage_set_default_path(app->ble_session->bt);
    bt_profile_restore_default(app->ble_session->bt);
    furi_record_close(RECORD_BT);
    free(app->ble_session);
    app->ble_session = NULL;
}
