#include "hid_helper.h"

static void bt_status_changed_callback(BtStatus status, void* context) {
    App* app = context;
    app->ble_connected = (status == BtStatusConnected);
    if(app->notifications) {
        notification_internal_message(
            app->notifications,
            app->ble_connected ? &sequence_set_blue_255 : &sequence_reset_blue);
    }
    if(app->view_port) {
        view_port_update(app->view_port);
    }
}

void hid_init(App* app) {
    if(app->transport == TransportUsb) {
        app->usb_mode_prev = furi_hal_usb_get_config();
        furi_hal_usb_unlock();
        furi_check(furi_hal_usb_set_config(&usb_hid, NULL));
    } else {
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH("bt_hid.keys"));
        BleProfileHidParams ble_params = {.device_name_prefix = "ClickRec", .mac_xor = 0};
        app->ble_profile =
            bt_profile_start(app->bt, ble_profile_hid, (FuriHalBleProfileParams)&ble_params);
        furi_check(app->ble_profile);
        furi_hal_bt_start_advertising();
        bt_set_status_changed_callback(app->bt, bt_status_changed_callback, app);
        app->ble_connected = false;
    }
}

void hid_deinit(App* app) {
    if(app->transport == TransportUsb) {
        furi_hal_usb_set_config(app->usb_mode_prev, NULL);
    } else {
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_keys_storage_set_default_path(app->bt);
        furi_check(bt_profile_restore_default(app->bt));
        app->ble_profile = NULL;
        app->ble_connected = false;
        if(app->notifications) {
            notification_internal_message(app->notifications, &sequence_reset_blue);
        }
    }
}

void hid_mouse_move(App* app, int8_t dx, int8_t dy) {
    if(app->transport == TransportUsb) {
        furi_hal_hid_mouse_move(dx, dy);
    } else {
        ble_profile_hid_mouse_move(app->ble_profile, dx, dy);
    }
}

void hid_mouse_press(App* app) {
    if(app->transport == TransportUsb) {
        furi_hal_hid_mouse_press(HID_MOUSE_BTN_LEFT);
    } else {
        ble_profile_hid_mouse_press(app->ble_profile, HID_MOUSE_BTN_LEFT);
    }
}

void hid_mouse_release(App* app) {
    if(app->transport == TransportUsb) {
        furi_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
    } else {
        ble_profile_hid_mouse_release(app->ble_profile, HID_MOUSE_BTN_LEFT);
    }
}

void hid_mouse_click(App* app) {
    hid_mouse_press(app);
    furi_delay_ms(CLICK_HOLD_MS);
    hid_mouse_release(app);
}

bool hid_is_connected(App* app) {
    if(app->transport == TransportUsb) {
        return furi_hal_hid_is_connected();
    } else {
        return app->ble_profile != NULL;
    }
}

void hid_mouse_press_btn(App* app, uint8_t btn) {
    if(app->transport == TransportUsb) {
        furi_hal_hid_mouse_press(btn);
    } else {
        ble_profile_hid_mouse_press(app->ble_profile, btn);
    }
}

void hid_mouse_release_btn(App* app, uint8_t btn) {
    if(app->transport == TransportUsb) {
        furi_hal_hid_mouse_release(btn);
    } else {
        ble_profile_hid_mouse_release(app->ble_profile, btn);
    }
}

void hid_mouse_release_all(App* app) {
    uint8_t all = HID_MOUSE_BTN_LEFT | HID_MOUSE_BTN_RIGHT | HID_MOUSE_BTN_WHEEL;
    hid_mouse_release_btn(app, all);
}
