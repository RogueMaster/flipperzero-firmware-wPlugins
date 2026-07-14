#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

#define TAG "BleClicker"

// Short vibration sequence for key feedback
static const NotificationSequence sequence_vibro_short = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

typedef struct {
    FuriMessageQueue* queue;
    ViewPort* viewport;
    Gui* gui;
    Bt* bt;
    FuriHalBleProfileBase* profile;
    NotificationApp* notifications;
    bool connected;
    bool alt_space_held;
} BleClicker;

// --- Callbacks ---

static void input_cb(InputEvent* event, void* ctx) {
    BleClicker* app = ctx;
    furi_message_queue_put(app->queue, event, 0);
}

static void draw_cb(Canvas* canvas, void* ctx) {
    BleClicker* app = ctx;

    canvas_clear(canvas);

    if(app->alt_space_held) {
        // Dictation mode: inverted display
        canvas_draw_box(canvas, 0, 0, 64, 128);
        canvas_set_color(canvas, ColorWhite);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, 55, AlignCenter, AlignCenter, "DICTATING");
        canvas_draw_str_aligned(canvas, 32, 72, AlignCenter, AlignCenter, "...");

        canvas_set_color(canvas, ColorBlack);
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 8, AlignCenter, AlignCenter, "Diana Remote");

    canvas_set_font(canvas, FontSecondary);
    if(app->connected) {
        canvas_draw_str_aligned(canvas, 32, 22, AlignCenter, AlignCenter, "Connected");
    } else {
        canvas_draw_str_aligned(canvas, 32, 22, AlignCenter, AlignCenter, "Waiting...");
    }

    // Separator
    canvas_draw_line(canvas, 4, 30, 60, 30);

    // Button legend (physical orientation)
    canvas_set_font(canvas, FontSecondary);
    int y = 42;
    canvas_draw_str(canvas, 2, y, "[OK] dict / Enter");
    y += 14;
    canvas_draw_str(canvas, 2, y, "[R]  next pane");
    y += 14;
    canvas_draw_str(canvas, 2, y, "[L]  next tab");
    y += 14;
    canvas_draw_str(canvas, 2, y, "[Up] Escape");
    y += 14;
    canvas_draw_str(canvas, 2, y, "[Dn] new Claude");

    // Separator
    canvas_draw_line(canvas, 4, y + 6, 60, y + 6);

    canvas_draw_str_aligned(canvas, 32, y + 16, AlignCenter, AlignCenter, "[Back] Exit");
}

static void bt_status_cb(BtStatus status, void* ctx) {
    BleClicker* app = ctx;
    app->connected = (status == BtStatusConnected);
    view_port_update(app->viewport);
}

// --- Key helpers ---

static void vibro(BleClicker* app) {
    notification_message(app->notifications, &sequence_vibro_short);
}

static void key_tap(BleClicker* app, uint16_t combo) {
    if(!app->connected || !app->profile) return;
    vibro(app);
    ble_profile_hid_kb_press(app->profile, combo);
    furi_delay_ms(30);
    ble_profile_hid_kb_release(app->profile, combo);
}

static void key_press(BleClicker* app, uint16_t combo) {
    if(!app->connected || !app->profile) return;
    vibro(app);
    ble_profile_hid_kb_press(app->profile, combo);
}

static void key_release(BleClicker* app, uint16_t combo) {
    if(!app->profile) return;
    ble_profile_hid_kb_release(app->profile, combo);
}

// --- Macro: type a string via HID ---

static void type_string(BleClicker* app, const char* str) {
    if(!app->connected || !app->profile) return;

    for(const char* p = str; *p; p++) {
        uint16_t key = 0;
        bool shift = false;

        if(*p >= 'a' && *p <= 'z') {
            key = HID_KEYBOARD_A + (*p - 'a');
        } else if(*p >= 'A' && *p <= 'Z') {
            key = HID_KEYBOARD_A + (*p - 'A');
            shift = true;
        } else if(*p >= '1' && *p <= '9') {
            key = HID_KEYBOARD_1 + (*p - '1');
        } else if(*p == '0') {
            key = HID_KEYBOARD_0;
        } else if(*p == ' ') {
            key = HID_KEYBOARD_SPACEBAR;
        } else if(*p == '-') {
            key = HID_KEYBOARD_MINUS;
        } else if(*p == '\n') {
            key = HID_KEYBOARD_RETURN;
        } else {
            continue;
        }

        if(shift) {
            key |= KEY_MOD_LEFT_SHIFT;
        }

        ble_profile_hid_kb_press(app->profile, key);
        furi_delay_ms(20);
        ble_profile_hid_kb_release(app->profile, key);
        furi_delay_ms(20);
    }
}

// --- BLE lifecycle ---

static void ble_init(BleClicker* app) {
    app->bt = furi_record_open(RECORD_BT);

    bt_disconnect(app->bt);
    furi_delay_ms(200);

    bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH(".ble_clicker.keys"));

    BleProfileHidParams params = {
        .device_name_prefix = "Diana",
        .mac_xor = 0x0001,
    };

    app->profile = bt_profile_start(app->bt, ble_profile_hid, (BleProfileHidParams*)&params);
    furi_check(app->profile);

    furi_hal_bt_start_advertising();
    bt_set_status_changed_callback(app->bt, bt_status_cb, app);
}

static void ble_deinit(BleClicker* app) {
    bt_set_status_changed_callback(app->bt, NULL, NULL);

    // Stop advertising before disconnect
    furi_hal_bt_stop_advertising();
    furi_delay_ms(100);

    bt_disconnect(app->bt);
    furi_delay_ms(200);

    // Restore default storage and profile
    bt_keys_storage_set_default_path(app->bt);
    furi_check(bt_profile_restore_default(app->bt));

    // Restart advertising for default (serial) profile
    furi_hal_bt_start_advertising();

    furi_record_close(RECORD_BT);
}

// --- Entry point ---

int32_t ble_clicker_app(void* p) {
    UNUSED(p);

    BleClicker* app = malloc(sizeof(BleClicker));
    memset(app, 0, sizeof(BleClicker));

    // Notifications (vibro)
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // GUI
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->viewport = view_port_alloc();
    view_port_set_orientation(app->viewport, ViewPortOrientationVertical);
    view_port_draw_callback_set(app->viewport, draw_cb, app);
    view_port_input_callback_set(app->viewport, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);

    // BLE
    ble_init(app);

    // Event loop
    InputEvent event;
    bool running = true;

    const uint16_t ALT_SPACE = KEY_MOD_LEFT_ALT | HID_KEYBOARD_SPACEBAR;

    while(running) {
        if(furi_message_queue_get(app->queue, &event, 100) != FuriStatusOk) {
            continue;
        }

        switch(event.key) {
        case InputKeyOk:
            // Hold: Opt+Space (dictation), Tap: Enter
            if(event.type == InputTypeLong) {
                key_press(app, ALT_SPACE);
                app->alt_space_held = true;
            } else if(event.type == InputTypeRelease && app->alt_space_held) {
                key_release(app, ALT_SPACE);
                app->alt_space_held = false;
            } else if(event.type == InputTypeShort) {
                key_tap(app, HID_KEYBOARD_RETURN);
            }
            break;

        case InputKeyUp:
            // Physical right: cycle next pane (Cmd+])
            if(event.type == InputTypeShort) {
                key_tap(app, KEY_MOD_LEFT_GUI | HID_KEYBOARD_CLOSE_BRACKET);
            }
            break;

        case InputKeyDown:
            // Physical left: cycle next tab (Cmd+Shift+])
            if(event.type == InputTypeShort) {
                key_tap(app, KEY_MOD_LEFT_GUI | KEY_MOD_LEFT_SHIFT | HID_KEYBOARD_CLOSE_BRACKET);
            }
            break;

        case InputKeyRight:
            // Physical up: Escape
            if(event.type == InputTypeShort) {
                key_tap(app, HID_KEYBOARD_ESCAPE);
            }
            break;

        case InputKeyLeft:
            // Physical down: new pane + launch diana claude --recent
            if(event.type == InputTypeShort) {
                vibro(app);
                // Cmd+D: split pane in iTerm2
                key_tap(app, KEY_MOD_LEFT_GUI | HID_KEYBOARD_D);
                furi_delay_ms(1000);
                // Type command and execute
                type_string(app, "diana claude --recent\n");
            }
            break;

        case InputKeyBack:
            if(event.type == InputTypeShort || event.type == InputTypeLong) {
                running = false;
            }
            break;

        default:
            break;
        }

        view_port_update(app->viewport);
    }

    // Release any held keys
    if(app->alt_space_held) {
        key_release(app, ALT_SPACE);
    }

    // Cleanup
    ble_deinit(app);

    view_port_enabled_set(app->viewport, false);
    gui_remove_view_port(app->gui, app->viewport);
    view_port_free(app->viewport);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    furi_message_queue_free(app->queue);
    free(app);

    return 0;
}
