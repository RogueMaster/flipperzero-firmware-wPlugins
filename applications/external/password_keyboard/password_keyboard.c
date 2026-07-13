#include "zk_vault.h"
#include "zk_settings.h"

#include <applications/services/bt/bt_service/bt.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <lib/ble_profile/extra_profiles/hid_profile.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define TAG             "PasswordKeyboard"
#define ZK_BT_KEYS_PATH APP_DATA_PATH(".bt_hid.keys")

typedef enum {
    ZkScreenConnect,
    ZkScreenMain,
    ZkScreenCreate,
    ZkScreenCreateConfirm,
    ZkScreenCreateAfterSend,
    ZkScreenPassword,
    ZkScreenViewPassword,
    ZkScreenPasswordActions,
    ZkScreenDeleteConfirm,
    ZkScreenSettings,
} ZkScreen;

typedef enum {
    ZkEventInput,
    ZkEventBluetooth,
} ZkEventType;

typedef struct {
    ZkEventType type;
    union {
        InputEvent input;
        BtStatus bluetooth;
    } data;
} ZkEvent;

typedef struct {
    Gui* gui;
    ViewPort* viewport;
    FuriMessageQueue* queue;
    Bt* bt;
    FuriHalBleProfileBase* profile;
    ZkVault vault;
    ZkSettings settings;
    ZkScreen screen;
    bool running;
    bool connected;
    uint8_t selection;
    uint8_t password_index;
    uint8_t create_classes;
    uint8_t create_length;
    bool create_hidden;
    bool create_manual;
    uint8_t create_daily_limit;
    char create_name[ZK_NAME_LENGTH];
    char generated[ZK_MAX_PASSWORD_LENGTH + 1];
    char revealed[ZK_MAX_PASSWORD_LENGTH + 1];
    char message[36];
    ViewDispatcher* text_dispatcher;
    bool text_input_accepted;
} ZkApp;

static void zk_draw_header(Canvas* canvas, ZkApp* app, const char* title) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 126, 9, AlignRight, AlignBottom, app->connected ? "BT connected" : "BT waiting");
    canvas_draw_line(canvas, 0, 13, 127, 13);
}

static void
    zk_draw_footer(Canvas* canvas, const char* left, const char* center, const char* right) {
    canvas_set_font(canvas, FontSecondary);
    if(left) canvas_draw_str_aligned(canvas, 2, 63, AlignLeft, AlignBottom, left);
    if(center) canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, center);
    if(right) canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, right);
}

static void
    zk_draw_row(Canvas* canvas, uint8_t y, bool selected, const char* label, const char* value) {
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y - 9, 128, 12);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, y, label);
    if(value) canvas_draw_str_aligned(canvas, 124, y, AlignRight, AlignBottom, value);
    canvas_set_color(canvas, ColorBlack);
}

static void zk_draw_scrolling_list(
    Canvas* canvas,
    const char* const* labels,
    const char* const* values,
    uint8_t count,
    uint8_t selection) {
    const uint8_t visible = 4;
    uint8_t start = 0;
    if(selection >= visible) start = selection - visible + 1;
    for(uint8_t row = 0; row < visible && start + row < count; row++) {
        zk_draw_row(
            canvas,
            25 + row * 11,
            start + row == selection,
            labels[start + row],
            values ? values[start + row] : NULL);
    }
    if(count > visible) elements_scrollbar(canvas, selection, count);
}

static void zk_draw_connect(Canvas* canvas, ZkApp* app) {
    UNUSED(app);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignCenter, "Password Keyboard");
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(
        canvas, 64, 27, AlignCenter, AlignTop, "Connect to PassKey\nin Bluetooth settings");
    elements_button_center(canvas, "Waiting...");
    zk_draw_footer(canvas, "Back exits", NULL, NULL);
}

static void zk_draw_main(Canvas* canvas, ZkApp* app) {
    zk_draw_header(canvas, app, "Passwords");
    const uint8_t count = app->vault.count + 2;
    const char* labels[ZK_MAX_PASSWORDS + 2];
    labels[0] = "+ Create new";
    for(uint8_t i = 0; i < app->vault.count; i++)
        labels[i + 1] = app->vault.records[i].name;
    labels[count - 1] = "Settings";
    zk_draw_scrolling_list(canvas, labels, NULL, count, app->selection);
    if(app->vault.count && count <= 3) zk_draw_footer(canvas, NULL, NULL, "Hold OK: edit");
}

static void zk_draw_create(Canvas* canvas, ZkApp* app) {
    zk_draw_header(canvas, app, "New password");
    const char* labels[] = {
        "Lowercase",
        "Uppercase",
        "Numbers",
        "Special",
        "Length",
        "Type",
        "Uses/day",
        "Continue",
        "Manual"};
    char length[8];
    char daily_limit[8];
    snprintf(length, sizeof(length), "%u", app->create_length);
    snprintf(daily_limit, sizeof(daily_limit), "%u", app->create_daily_limit);
    const char* values[] = {
        app->create_classes & ZkClassLower ? "yes" : "no",
        app->create_classes & ZkClassUpper ? "yes" : "no",
        app->create_classes & ZkClassNumber ? "yes" : "no",
        app->create_classes & ZkClassSpecial ? "yes" : "no",
        length,
        app->create_hidden ? "hidden" : "normal",
        app->create_hidden ? daily_limit : "-",
        ">",
        ">",
    };
    zk_draw_scrolling_list(canvas, labels, values, 9, app->selection);
}

static void zk_draw_create_confirm(Canvas* canvas, ZkApp* app) {
    zk_draw_header(canvas, app, app->create_hidden ? "Hidden password" : "New password");
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(
        canvas,
        64,
        21,
        AlignCenter,
        AlignTop,
        app->create_manual ?
            "Manual password ready.\nFocus the password field." :
            (app->create_hidden ? "Generated and concealed.\nFocus the password field." :
                                  "Generated. Focus the target\npassword field before typing."));
    if(app->message[0])
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, app->message);
    elements_button_center(canvas, "Type first time");
}

static void zk_draw_create_after_send(Canvas* canvas, ZkApp* app) {
    zk_draw_header(canvas, app, "Password entered");
    char usage[16];
    if(app->create_hidden) {
        snprintf(usage, sizeof(usage), "%u/day", app->create_daily_limit);
    } else {
        strlcpy(usage, "", sizeof(usage));
    }
    const char* labels[] = {"Type again", "Press Tab", "Press Enter", "Save password"};
    const char* values[] = {usage[0] ? usage : NULL, NULL, NULL, NULL};
    zk_draw_scrolling_list(canvas, labels, values, 4, app->selection);
}

static void zk_draw_password(Canvas* canvas, ZkApp* app) {
    ZkPasswordRecord* record = &app->vault.records[app->password_index];
    zk_draw_header(canvas, app, record->name);
    if(record->flags & ZkPasswordHidden) {
        char remaining[32];
        snprintf(
            remaining,
            sizeof(remaining),
            "%u of %u uses left today",
            zk_vault_remaining(&app->vault, app->password_index),
            record->daily_limit);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignCenter, remaining);
        zk_draw_row(canvas, 45, true, "Type password", NULL);
    } else {
        const char* labels[] = {"Type password", "View password"};
        zk_draw_row(canvas, 32, app->selection == 0, labels[0], NULL);
        zk_draw_row(canvas, 45, app->selection == 1, labels[1], NULL);
    }
    if(app->message[0]) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, app->message);
    }
}

static void zk_draw_revealed(Canvas* canvas, ZkApp* app) {
    zk_draw_header(canvas, app, "Visible password");
    canvas_set_font(canvas, FontSecondary);
    const size_t length = strlen(app->revealed);
    for(uint8_t row = 0; row < 4; row++) {
        const size_t offset = row * 20;
        if(offset >= length) break;
        char line[21];
        size_t take = length - offset;
        if(take > 20) take = 20;
        memcpy(line, app->revealed + offset, take);
        line[take] = '\0';
        canvas_draw_str(canvas, 3, 25 + row * 10, line);
    }
    zk_draw_footer(canvas, "Back hides", NULL, NULL);
}

static void zk_draw_password_actions(Canvas* canvas, ZkApp* app) {
    zk_draw_header(canvas, app, app->vault.records[app->password_index].name);
    const char* labels[] = {"Rename", "Delete"};
    zk_draw_row(canvas, 32, app->selection == 0, labels[0], NULL);
    zk_draw_row(canvas, 45, app->selection == 1, labels[1], NULL);
    if(app->message[0]) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, app->message);
    }
}

static void zk_draw_delete_confirm(Canvas* canvas, ZkApp* app) {
    zk_draw_header(canvas, app, "Delete password?");
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(
        canvas, 64, 24, AlignCenter, AlignTop, app->vault.records[app->password_index].name);
    canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignBottom, "This cannot be undone");
    elements_button_center(canvas, "Delete");
    zk_draw_footer(canvas, "Back cancels", NULL, NULL);
}

static void zk_draw_settings(Canvas* canvas, ZkApp* app) {
    zk_draw_header(canvas, app, "Settings");
    char default_name[14];
    strlcpy(default_name, app->settings.default_name, sizeof(default_name));
    zk_draw_row(canvas, 29, true, "Default name", default_name);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "Idea by: Evgeniy Raev");
    if(app->message[0]) {
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, app->message);
    }
    zk_draw_footer(canvas, "Back", "OK Edit", NULL);
}

static void zk_draw_callback(Canvas* canvas, void* context) {
    ZkApp* app = context;
    canvas_clear(canvas);
    switch(app->screen) {
    case ZkScreenConnect:
        zk_draw_connect(canvas, app);
        break;
    case ZkScreenMain:
        zk_draw_main(canvas, app);
        break;
    case ZkScreenCreate:
        zk_draw_create(canvas, app);
        break;
    case ZkScreenCreateConfirm:
        zk_draw_create_confirm(canvas, app);
        break;
    case ZkScreenCreateAfterSend:
        zk_draw_create_after_send(canvas, app);
        break;
    case ZkScreenPassword:
        zk_draw_password(canvas, app);
        break;
    case ZkScreenViewPassword:
        zk_draw_revealed(canvas, app);
        break;
    case ZkScreenPasswordActions:
        zk_draw_password_actions(canvas, app);
        break;
    case ZkScreenDeleteConfirm:
        zk_draw_delete_confirm(canvas, app);
        break;
    case ZkScreenSettings:
        zk_draw_settings(canvas, app);
        break;
    }
}

static void zk_input_callback(InputEvent* input, void* context) {
    ZkApp* app = context;
    ZkEvent event = {.type = ZkEventInput, .data.input = *input};
    furi_message_queue_put(app->queue, &event, 0);
}

static void zk_bt_callback(BtStatus status, void* context) {
    ZkApp* app = context;
    ZkEvent event = {.type = ZkEventBluetooth, .data.bluetooth = status};
    furi_message_queue_put(app->queue, &event, 0);
}

static void zk_set_screen(ZkApp* app, ZkScreen screen) {
    if(app->screen == ZkScreenViewPassword) zk_crypto_wipe(app->revealed, sizeof(app->revealed));
    app->screen = screen;
    app->selection = 0;
    app->message[0] = '\0';
}

static void zk_cancel_creation(ZkApp* app) {
    zk_crypto_wipe(app->generated, sizeof(app->generated));
    zk_set_screen(app, ZkScreenMain);
}

static void zk_text_input_done(void* context) {
    ZkApp* app = context;
    app->text_input_accepted = true;
    view_dispatcher_stop(app->text_dispatcher);
}

static bool zk_text_input_back(void* context) {
    ZkApp* app = context;
    app->text_input_accepted = false;
    view_dispatcher_stop(app->text_dispatcher);
    return true;
}

static bool zk_prompt_text(
    ZkApp* app,
    const char* header,
    char* buffer,
    size_t buffer_size,
    bool clear_default) {
    TextInput* text_input = text_input_alloc();
    app->text_dispatcher = view_dispatcher_alloc();
    app->text_input_accepted = false;

    text_input_set_header_text(text_input, header);
    text_input_set_minimum_length(text_input, 1);
    text_input_set_result_callback(
        text_input, zk_text_input_done, app, buffer, buffer_size, clear_default);

    gui_remove_view_port(app->gui, app->viewport);
    view_dispatcher_set_event_callback_context(app->text_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->text_dispatcher, zk_text_input_back);
    view_dispatcher_add_view(app->text_dispatcher, 0, text_input_get_view(text_input));
    view_dispatcher_attach_to_gui(app->text_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->text_dispatcher, 0);
    view_dispatcher_run(app->text_dispatcher);

    view_dispatcher_remove_view(app->text_dispatcher, 0);
    text_input_free(text_input);
    view_dispatcher_free(app->text_dispatcher);
    app->text_dispatcher = NULL;
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);
    return app->text_input_accepted;
}

static bool zk_hid_release_all_with_retry(ZkApp* app) {
    for(uint8_t attempt = 0; attempt < 4; attempt++) {
        /* Firmware 1.4.3 / API 87.1 propagates ble_gatt_characteristic_update,
         * whose implementation returns false for BLE_STATUS_SUCCESS. */
        if(!ble_profile_hid_kb_release_all(app->profile)) return true;
        furi_delay_ms(35);
    }
    return false;
}

static bool zk_hid_press_with_retry(ZkApp* app, uint16_t key) {
    for(uint8_t attempt = 0; attempt < 4; attempt++) {
        if(!ble_profile_hid_kb_press(app->profile, key)) return true;
        /* kb_press changes the in-memory report even when the GATT update
         * fails. Clear it before retrying so the key is not duplicated. */
        zk_hid_release_all_with_retry(app);
        furi_delay_ms(35);
    }
    return false;
}

static bool zk_type_password(ZkApp* app, const char* password) {
    if(!app->connected || !app->profile) {
        strlcpy(app->message, "Bluetooth not connected", sizeof(app->message));
        return false;
    }

    /* Give the host time to finish subscribing to HID reports and clear the
     * profile's newly allocated keyboard report before its first use. */
    furi_delay_ms(100);
    if(!zk_hid_release_all_with_retry(app)) {
        strlcpy(app->message, "Host HID not ready", sizeof(app->message));
        FURI_LOG_E(TAG, "HID reset failed");
        return false;
    }

    for(const char* cursor = password; *cursor; cursor++) {
        const uint16_t key = HID_ASCII_TO_KEY(*cursor);
        if(key == HID_KEYBOARD_NONE) {
            strlcpy(app->message, "Unsupported character", sizeof(app->message));
            FURI_LOG_E(TAG, "Unsupported character 0x%02X", (uint8_t)*cursor);
            return false;
        }
        if(!zk_hid_press_with_retry(app, key)) {
            strlcpy(app->message, "Host rejected key press", sizeof(app->message));
            FURI_LOG_E(TAG, "HID press failed for 0x%02X", (uint8_t)*cursor);
            return false;
        }
        furi_delay_ms(30);
        if(ble_profile_hid_kb_release(app->profile, key)) {
            if(!zk_hid_release_all_with_retry(app)) {
                strlcpy(app->message, "Host rejected key release", sizeof(app->message));
                FURI_LOG_E(TAG, "HID release failed for 0x%02X", (uint8_t)*cursor);
                return false;
            }
        }
        furi_delay_ms(30);
    }
    app->message[0] = '\0';
    return true;
}

static bool zk_tap_hid_key(ZkApp* app, uint16_t key, const char* success_message) {
    if(!app->connected || !app->profile) {
        strlcpy(app->message, "Bluetooth not connected", sizeof(app->message));
        return false;
    }
    if(!zk_hid_press_with_retry(app, key)) {
        strlcpy(app->message, "Host rejected key press", sizeof(app->message));
        return false;
    }
    furi_delay_ms(30);
    if(ble_profile_hid_kb_release(app->profile, key) && !zk_hid_release_all_with_retry(app)) {
        strlcpy(app->message, "Host rejected key release", sizeof(app->message));
        return false;
    }
    strlcpy(app->message, success_message, sizeof(app->message));
    return true;
}

static void zk_save_created(ZkApp* app) {
    if(zk_vault_add(
           &app->vault,
           app->create_name,
           app->generated,
           app->create_hidden,
           app->create_daily_limit,
           0)) {
        zk_cancel_creation(app);
    } else {
        strlcpy(app->message, "Could not save", sizeof(app->message));
    }
}

static void zk_adjust_limit(ZkApp* app, int8_t delta) {
    int8_t next = (int8_t)app->create_daily_limit + delta;
    if(next < 1) next = 9;
    if(next > 9) next = 1;
    app->create_daily_limit = next;
}

static void zk_handle_main(ZkApp* app, InputKey key) {
    const uint8_t count = app->vault.count + 2;
    if(key == InputKeyUp)
        app->selection = app->selection ? app->selection - 1 : count - 1;
    else if(key == InputKeyDown)
        app->selection = (app->selection + 1) % count;
    else if(key == InputKeyOk) {
        if(app->selection == 0) {
            app->create_classes = ZkClassLower | ZkClassUpper | ZkClassNumber;
            app->create_length = 20;
            app->create_hidden = false;
            app->create_manual = false;
            app->create_daily_limit = ZK_DEFAULT_DAILY_LIMIT;
            strlcpy(app->create_name, app->settings.default_name, sizeof(app->create_name));
            if(zk_prompt_text(
                   app, "Name the password", app->create_name, sizeof(app->create_name), true)) {
                zk_set_screen(app, ZkScreenCreate);
            }
        } else if(app->selection == count - 1) {
            zk_set_screen(app, ZkScreenSettings);
        } else {
            app->password_index = app->selection - 1;
            zk_set_screen(app, ZkScreenPassword);
        }
    } else if(key == InputKeyBack) {
        app->running = false;
    }
}

static void zk_handle_create(ZkApp* app, InputKey key) {
    if(key == InputKeyUp)
        app->selection = app->selection ? app->selection - 1 : 8;
    else if(key == InputKeyDown)
        app->selection = (app->selection + 1) % 9;
    else if(key == InputKeyBack)
        zk_cancel_creation(app);
    else if(key == InputKeyLeft || key == InputKeyRight || key == InputKeyOk) {
        if(app->selection < 4) {
            app->create_classes ^= (1U << app->selection);
        } else if(app->selection == 4) {
            int8_t delta = key == InputKeyLeft ? -1 : 1;
            int16_t next = (int16_t)app->create_length + delta;
            if(next < 4) next = ZK_MAX_PASSWORD_LENGTH;
            if(next > ZK_MAX_PASSWORD_LENGTH) next = 4;
            app->create_length = next;
        } else if(app->selection == 5) {
            app->create_hidden = !app->create_hidden;
        } else if(app->selection == 6) {
            if(app->create_hidden) zk_adjust_limit(app, key == InputKeyLeft ? -1 : 1);
        } else if(app->selection == 7 && key == InputKeyOk) {
            uint8_t class_count = 0;
            for(uint8_t i = 0; i < 4; i++)
                class_count += !!(app->create_classes & (1U << i));
            if(!class_count || app->create_length < class_count) return;
            if(zk_password_generate(app->generated, app->create_length, app->create_classes)) {
                app->create_manual = false;
                zk_set_screen(app, ZkScreenCreateConfirm);
            }
        } else if(app->selection == 8 && key == InputKeyOk) {
            zk_crypto_wipe(app->generated, sizeof(app->generated));
            if(zk_prompt_text(
                   app, "Enter the password", app->generated, sizeof(app->generated), false)) {
                app->create_manual = true;
                zk_set_screen(app, ZkScreenCreateConfirm);
            } else {
                zk_crypto_wipe(app->generated, sizeof(app->generated));
            }
        }
    }
}

static void zk_handle_create_confirm(ZkApp* app, InputKey key) {
    if(key == InputKeyBack) {
        zk_cancel_creation(app);
    } else if(key == InputKeyOk) {
        if(!app->connected) {
            strlcpy(app->message, "Connect Bluetooth first", sizeof(app->message));
        } else if(zk_type_password(app, app->generated)) {
            zk_set_screen(app, ZkScreenCreateAfterSend);
        }
    }
}

static void zk_handle_create_after_send(ZkApp* app, InputKey key) {
    if(key == InputKeyUp)
        app->selection = app->selection ? app->selection - 1 : 3;
    else if(key == InputKeyDown)
        app->selection = (app->selection + 1) % 4;
    else if(key == InputKeyBack)
        zk_cancel_creation(app);
    else if(key == InputKeyOk && app->selection == 0) {
        zk_type_password(app, app->generated);
    } else if(key == InputKeyOk && app->selection == 1) {
        zk_tap_hid_key(app, HID_KEYBOARD_TAB, "Tab sent");
    } else if(key == InputKeyOk && app->selection == 2) {
        zk_tap_hid_key(app, HID_KEYBOARD_RETURN, "Enter sent");
    } else if(key == InputKeyOk && app->selection == 3) {
        zk_save_created(app);
    }
}

static void zk_handle_password(ZkApp* app, InputKey key) {
    ZkPasswordRecord* record = &app->vault.records[app->password_index];
    const bool hidden = record->flags & ZkPasswordHidden;
    if(key == InputKeyBack) {
        zk_set_screen(app, ZkScreenMain);
    } else if(!hidden && (key == InputKeyUp || key == InputKeyDown)) {
        app->selection ^= 1;
    } else if(key == InputKeyOk && !hidden && app->selection == 1) {
        if(zk_vault_decrypt(record, app->revealed))
            zk_set_screen(app, ZkScreenViewPassword);
        else
            strlcpy(app->message, "Vault data is damaged", sizeof(app->message));
    } else if(key == InputKeyOk) {
        if(!app->connected) {
            strlcpy(app->message, "Connect Bluetooth first", sizeof(app->message));
            return;
        }
        if(hidden && zk_vault_remaining(&app->vault, app->password_index) == 0) {
            strlcpy(app->message, "No uses left today", sizeof(app->message));
            return;
        }
        if(!zk_vault_decrypt(record, app->revealed)) {
            strlcpy(app->message, "Vault data is damaged", sizeof(app->message));
            return;
        }
        const bool sent = zk_type_password(app, app->revealed);
        zk_crypto_wipe(app->revealed, sizeof(app->revealed));
        if(sent) {
            if(hidden && !zk_vault_consume(&app->vault, app->password_index)) {
                strlcpy(app->message, "Entered; counter save failed", sizeof(app->message));
            } else {
                strlcpy(app->message, "Password entered", sizeof(app->message));
            }
        }
    }
}

static void zk_handle_password_actions(ZkApp* app, InputKey key) {
    if(key == InputKeyUp || key == InputKeyDown) {
        app->selection ^= 1;
    } else if(key == InputKeyBack) {
        zk_set_screen(app, ZkScreenMain);
    } else if(key == InputKeyOk && app->selection == 0) {
        strlcpy(
            app->create_name,
            app->vault.records[app->password_index].name,
            sizeof(app->create_name));
        if(zk_prompt_text(
               app, "Rename password", app->create_name, sizeof(app->create_name), false)) {
            if(zk_vault_rename(&app->vault, app->password_index, app->create_name)) {
                zk_set_screen(app, ZkScreenMain);
            } else {
                strlcpy(app->message, "Rename failed", sizeof(app->message));
            }
        }
    } else if(key == InputKeyOk && app->selection == 1) {
        zk_set_screen(app, ZkScreenDeleteConfirm);
    }
}

static void zk_handle_settings(ZkApp* app, InputKey key) {
    if(key == InputKeyBack) {
        zk_set_screen(app, ZkScreenMain);
    } else if(key == InputKeyOk) {
        strlcpy(app->create_name, app->settings.default_name, sizeof(app->create_name));
        if(zk_prompt_text(
               app, "Default password name", app->create_name, sizeof(app->create_name), false)) {
            char previous[ZK_NAME_LENGTH];
            strlcpy(previous, app->settings.default_name, sizeof(previous));
            strlcpy(
                app->settings.default_name, app->create_name, sizeof(app->settings.default_name));
            if(zk_settings_save(&app->settings)) {
                strlcpy(app->message, "Default name saved", sizeof(app->message));
            } else {
                strlcpy(app->settings.default_name, previous, sizeof(app->settings.default_name));
                strlcpy(app->message, "Settings save failed", sizeof(app->message));
            }
        }
    }
}

static void zk_handle_input(ZkApp* app, InputEvent input) {
    if(input.type == InputTypeLong && input.key == InputKeyOk) {
        if(app->screen == ZkScreenMain && app->selection > 0 &&
           app->selection <= app->vault.count) {
            app->password_index = app->selection - 1;
            zk_set_screen(app, ZkScreenPasswordActions);
        }
        return;
    }
    if(input.type != InputTypeShort && input.type != InputTypeRepeat) return;
    switch(app->screen) {
    case ZkScreenConnect:
        if(input.key == InputKeyBack) app->running = false;
        break;
    case ZkScreenMain:
        zk_handle_main(app, input.key);
        break;
    case ZkScreenCreate:
        zk_handle_create(app, input.key);
        break;
    case ZkScreenCreateConfirm:
        zk_handle_create_confirm(app, input.key);
        break;
    case ZkScreenCreateAfterSend:
        zk_handle_create_after_send(app, input.key);
        break;
    case ZkScreenPassword:
        zk_handle_password(app, input.key);
        break;
    case ZkScreenViewPassword:
        if(input.key == InputKeyBack || input.key == InputKeyOk)
            zk_set_screen(app, ZkScreenPassword);
        break;
    case ZkScreenPasswordActions:
        zk_handle_password_actions(app, input.key);
        break;
    case ZkScreenDeleteConfirm:
        if(input.key == InputKeyBack) {
            zk_set_screen(app, ZkScreenPasswordActions);
        } else if(input.key == InputKeyOk) {
            if(!zk_vault_delete(&app->vault, app->password_index)) {
                zk_vault_load(&app->vault);
            }
            zk_set_screen(app, ZkScreenMain);
        }
        break;
    case ZkScreenSettings:
        zk_handle_settings(app, input.key);
        break;
    }
}

static bool zk_start_bluetooth(ZkApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    furi_record_close(RECORD_STORAGE);

    app->bt = furi_record_open(RECORD_BT);
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_storage_path(app->bt, ZK_BT_KEYS_PATH);
    const BleProfileHidParams profile_params = {
        .device_name_prefix = "PassKey",
        .mac_xor = 0x5A4B,
    };
    app->profile = bt_profile_start(app->bt, ble_profile_hid, (void*)&profile_params);
    if(!app->profile) return false;
    bt_set_status_changed_callback(app->bt, zk_bt_callback, app);
    furi_hal_bt_start_advertising();
    return true;
}

static void zk_stop_bluetooth(ZkApp* app) {
    if(!app->bt) return;
    bt_set_status_changed_callback(app->bt, NULL, NULL);
    if(app->profile) ble_profile_hid_kb_release_all(app->profile);
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(app->bt);
    bt_profile_restore_default(app->bt);
    furi_record_close(RECORD_BT);
    app->bt = NULL;
    app->profile = NULL;
}

int32_t password_keyboard_app(void* context) {
    UNUSED(context);
    ZkApp* app = malloc(sizeof(ZkApp));
    memset(app, 0, sizeof(*app));
    app->running = true;
    app->screen = ZkScreenConnect;
    zk_vault_load(&app->vault);
    zk_vault_import(&app->vault);
    zk_settings_load(&app->settings);

    app->queue = furi_message_queue_alloc(8, sizeof(ZkEvent));
    app->viewport = view_port_alloc();
    view_port_draw_callback_set(app->viewport, zk_draw_callback, app);
    view_port_input_callback_set(app->viewport, zk_input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);

    if(!zk_start_bluetooth(app)) {
        FURI_LOG_E(TAG, "Failed to start BLE HID profile");
        strlcpy(app->message, "Bluetooth unavailable", sizeof(app->message));
    }

    while(app->running) {
        ZkEvent event;
        if(furi_message_queue_get(app->queue, &event, 100) == FuriStatusOk) {
            if(event.type == ZkEventInput) {
                zk_handle_input(app, event.data.input);
            } else {
                app->connected = event.data.bluetooth == BtStatusConnected;
                if(app->connected && app->screen == ZkScreenConnect) {
                    zk_set_screen(app, ZkScreenMain);
                }
            }
            view_port_update(app->viewport);
        }
    }

    zk_crypto_wipe(app->generated, sizeof(app->generated));
    zk_crypto_wipe(app->revealed, sizeof(app->revealed));
    zk_stop_bluetooth(app);
    gui_remove_view_port(app->gui, app->viewport);
    furi_record_close(RECORD_GUI);
    view_port_free(app->viewport);
    furi_message_queue_free(app->queue);
    zk_crypto_wipe(app, sizeof(*app));
    free(app);
    return 0;
}
