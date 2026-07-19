#include "android_keyboard_bridge.h"

#include <string.h>

#include <furi_hal_bt.h>
#include <profiles/serial_profile.h>

#include "usb_hid_bridge.h"
#include "screenshot.h"

#define AKB_SERIAL_BUFFER_SIZE 128
#define AKB_HID_QUEUE_SIZE     64
/** Hold gap so the host sees press before release when both arrive back-to-back. */
#define AKB_HID_EVENT_GAP_MS   25

static uint16_t akb_serial_callback(SerialServiceEvent event, void* context);
static void akb_claim_serial_rx(AndroidKeyboardBridge* app);

static void akb_set_backlight_forced(AndroidKeyboardBridge* app, bool forced) {
    if(forced == app->backlight_forced) {
        return;
    }
    app->backlight_forced = forced;
    if(forced) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    } else {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    }
}

/**
 * Hardware button events from the input service (GPIO IRQ → debounce → pubsub).
 * Independent of ViewPort / GUI focus / main-loop HID delays.
 */
static void akb_input_events_callback(const void* value, void* context) {
    AndroidKeyboardBridge* app = context;
    const InputEvent* event = value;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong ||
        event->type == InputTypePress)) {
        app->exit_requested = true;
        return;
    }

    // Short only: Press+Short would double-toggle on one physical click.
    if(event->key == InputKeyUp && event->type == InputTypeShort) {
        akb_set_backlight_forced(app, !app->backlight_forced);
        return;
    }

#ifdef AKB_SCREENSHOT
    if(event->key == InputKeyDown && event->type == InputTypeShort) {
        akb_screenshot_on_down_short(app);
    }
#endif
}

static void akb_draw_callback(Canvas* canvas, void* ctx) {
    AndroidKeyboardBridge* app = ctx;
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Android KB Bridge");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 18, app->status_line);
    canvas_draw_str(canvas, 2, 30, app->usb_connected ? "USB: connected" : "USB: waiting");
    canvas_draw_str(
        canvas,
        2,
        42,
        app->ble_phone_connected ? "Phone: connected" : "Phone: waiting");

    char counter[32];
    snprintf(
        counter,
        sizeof(counter),
        "Frames:%lu HID:%lu",
        (unsigned long)app->frames_received,
        (unsigned long)app->hid_applied);
    canvas_draw_str(canvas, 2, 54, counter);
#ifdef AKB_SCREENSHOT
    canvas_draw_str(
        canvas,
        2,
        62,
        app->backlight_forced ? "Up=light* Dn3=shot Back" : "Up=light Dn3=shot Back");
#else
    canvas_draw_str(
        canvas,
        2,
        62,
        app->backlight_forced ? "Up=light* Back=exit" : "Up=light  Back=exit");
#endif
    furi_mutex_release(app->mutex);
}

static void akb_apply_hid_cmd(AndroidKeyboardBridge* app, const AkbHidCmd* cmd) {
    switch(cmd->type) {
    case AkbHidCmdKeyDown:
        usb_hid_key_down(cmd->mods, cmd->keycode);
        break;
    case AkbHidCmdKeyUp:
        usb_hid_key_up(cmd->mods, cmd->keycode);
        break;
    case AkbHidCmdMouseMove:
        usb_hid_mouse_move(cmd->dx, cmd->dy);
        break;
    case AkbHidCmdMouseButtonDown:
        usb_hid_mouse_button_down(cmd->mouse_button);
        break;
    case AkbHidCmdMouseButtonUp:
        usb_hid_mouse_button_up(cmd->mouse_button);
        break;
    case AkbHidCmdMouseScroll:
        usb_hid_mouse_scroll(cmd->scroll);
        break;
    case AkbHidCmdPanic:
        usb_hid_panic_release();
        break;
    }

    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    app->hid_applied++;
    if(cmd->type == AkbHidCmdKeyDown) {
        snprintf(
            app->status_line,
            sizeof(app->status_line),
            "HID down key=%02X",
            cmd->keycode);
    } else if(cmd->type == AkbHidCmdKeyUp) {
        snprintf(
            app->status_line,
            sizeof(app->status_line),
            "HID up key=%02X",
            cmd->keycode);
    } else if(cmd->type == AkbHidCmdMouseMove) {
        snprintf(app->status_line, sizeof(app->status_line), "Mouse %+d,%+d", cmd->dx, cmd->dy);
    } else if(
        cmd->type == AkbHidCmdMouseButtonDown || cmd->type == AkbHidCmdMouseButtonUp) {
        snprintf(
            app->status_line,
            sizeof(app->status_line),
            "Mouse btn %02X",
            cmd->mouse_button);
    } else if(cmd->type == AkbHidCmdMouseScroll) {
        snprintf(app->status_line, sizeof(app->status_line), "Scroll %+d", cmd->scroll);
    }
    furi_mutex_release(app->mutex);
}

static AndroidKeyboardBridge* akb_alloc(void) {
    AndroidKeyboardBridge* app = malloc(sizeof(AndroidKeyboardBridge));
    furi_check(app);
    memset(app, 0, sizeof(AndroidKeyboardBridge));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->hid_queue = furi_message_queue_alloc(AKB_HID_QUEUE_SIZE, sizeof(AkbHidCmd));
    app->bt = furi_record_open(RECORD_BT);
    app->profile = NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Subscribe to raw input pubsub (same path as GPIO button IRQs after debounce).
    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    app->exit_requested = false;
    app->backlight_forced = false;
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, akb_input_events_callback, app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, akb_draw_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_enabled_set(app->view_port, true);

    akb_protocol_init(&app->parser, app->hid_queue);
    app->ble_active = false;
    app->ble_phone_connected = false;
    app->ble_need_buffer_notify = false;
    app->reclaim_ticks = 0;
    app->usb_connected = false;
    app->frames_received = 0;
    app->hid_applied = 0;
    snprintf(app->status_line, sizeof(app->status_line), "Starting...");

    akb_screenshot_init(app);

    return app;
}

static void akb_free(AndroidKeyboardBridge* app) {
    // Restore normal backlight timeout if we locked it on.
    akb_set_backlight_forced(app, false);

    akb_screenshot_free(app);

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_pubsub_unsubscribe(app->input_events, app->input_subscription);
    furi_record_close(RECORD_INPUT_EVENTS);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);

    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->hid_queue);
    free(app);
}

static void akb_claim_serial_rx(AndroidKeyboardBridge* app) {
    if(!app->profile) {
        return;
    }

    ble_profile_serial_set_event_callback(
        app->profile, AKB_SERIAL_BUFFER_SIZE, akb_serial_callback, app);
    ble_profile_serial_set_rpc_active(app->profile, true);
    ble_profile_serial_notify_buffer_is_empty(app->profile);
}

static uint16_t akb_serial_callback(SerialServiceEvent event, void* context) {
    AndroidKeyboardBridge* app = context;

    if(event.event == SerialServiceEventTypeDataReceived) {
        // IMPORTANT: serial service already holds buff_size_mtx around this
        // callback. Do NOT call ble_profile_serial_notify_buffer_is_empty here —
        // that re-acquires the same mutex and deadlocks after the first packet.
        furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
        app->ble_phone_connected = true;
        app->ble_need_buffer_notify = true;
        app->frames_received += akb_protocol_feed(&app->parser, event.data.buffer, event.data.size);
        snprintf(app->status_line, sizeof(app->status_line), "Last RX: %u bytes", event.data.size);
        furi_mutex_release(app->mutex);
    }

    return AKB_SERIAL_BUFFER_SIZE;
}

static void akb_bt_status_changed(BtStatus status, void* context) {
    AndroidKeyboardBridge* app = context;

    if(status == BtStatusConnected) {
        akb_claim_serial_rx(app);
        app->reclaim_ticks = 20;
    }

    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    if(status == BtStatusConnected) {
        app->ble_phone_connected = true;
        snprintf(app->status_line, sizeof(app->status_line), "Phone linked — type now");
    } else if(status == BtStatusAdvertising) {
        app->ble_phone_connected = false;
        snprintf(app->status_line, sizeof(app->status_line), "Waiting for phone...");
    } else {
        app->ble_phone_connected = false;
        snprintf(app->status_line, sizeof(app->status_line), "BLE status: %d", (int)status);
    }
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void akb_start_ble(AndroidKeyboardBridge* app) {
    if(!furi_hal_bt_is_active()) {
        snprintf(app->status_line, sizeof(app->status_line), "Enable Bluetooth");
        return;
    }

    bt_set_status_changed_callback(app->bt, akb_bt_status_changed, app);

    bt_disconnect(app->bt);
    furi_delay_ms(200);

    app->profile = bt_profile_start(app->bt, ble_profile_serial, NULL);
    if(!app->profile) {
        snprintf(app->status_line, sizeof(app->status_line), "BLE profile failed");
        return;
    }

    akb_claim_serial_rx(app);
    furi_hal_bt_start_advertising();
    app->ble_active = true;
    app->ble_phone_connected = false;
    snprintf(app->status_line, sizeof(app->status_line), "Waiting for phone...");
}

static void akb_stop_ble(AndroidKeyboardBridge* app) {
    bt_set_status_changed_callback(app->bt, NULL, NULL);
    if(app->profile) {
        ble_profile_serial_set_rpc_active(app->profile, false);
        ble_profile_serial_set_event_callback(app->profile, 0, NULL, NULL);
        // Drop the phone link before restoring the default profile — otherwise
        // restore can sit in GAP teardown for a long time while still bonded/active.
        bt_disconnect(app->bt);
        bt_profile_restore_default(app->bt);
        app->profile = NULL;
    }
    app->ble_active = false;
    app->ble_phone_connected = false;
}

int32_t android_keyboard_bridge_app(void* p) {
    UNUSED(p);

    AndroidKeyboardBridge* app = akb_alloc();

    usb_hid_bridge_start();
    akb_start_ble(app);
    view_port_update(app->view_port);

    AkbHidCmd hid_cmd;
    while(!app->exit_requested) {
        const bool usb_now = usb_hid_bridge_is_usb_connected();
        app->usb_connected = usb_now;

        if(app->reclaim_ticks > 0) {
            akb_claim_serial_rx(app);
            app->reclaim_ticks--;
        }

        if(app->ble_need_buffer_notify && app->profile) {
            app->ble_need_buffer_notify = false;
            ble_profile_serial_notify_buffer_is_empty(app->profile);
        }

        akb_screenshot_poll(app);

        if(app->exit_requested) {
            break;
        }

        // No USB host: drain RX queue (do not apply HID) so Back teardown stays snappy.
        // Exit is Back-only — cable unplug must not leave the app.
        if(!usb_now) {
            while(furi_message_queue_get(app->hid_queue, &hid_cmd, 0) == FuriStatusOk) {
            }
            furi_delay_ms(5);
            view_port_update(app->view_port);
            continue;
        }

        if(furi_message_queue_get(app->hid_queue, &hid_cmd, 0) == FuriStatusOk) {
            const bool needs_gap = (hid_cmd.type == AkbHidCmdKeyDown ||
                                    hid_cmd.type == AkbHidCmdKeyUp);
            akb_apply_hid_cmd(app, &hid_cmd);
            if(needs_gap) {
                for(uint8_t i = 0; i < AKB_HID_EVENT_GAP_MS && !app->exit_requested; i++) {
                    furi_delay_ms(1);
                }
            }
        } else {
            furi_delay_ms(5);
        }

        view_port_update(app->view_port);
    }

    // Drop queued frames before teardown so BLE RX cannot keep us busy.
    while(furi_message_queue_get(app->hid_queue, &hid_cmd, 0) == FuriStatusOk) {
    }

    akb_stop_ble(app);
    usb_hid_bridge_stop();
    akb_free(app);
    return 0;
}
