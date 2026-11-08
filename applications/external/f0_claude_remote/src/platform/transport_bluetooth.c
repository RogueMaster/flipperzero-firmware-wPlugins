#include "transport_bluetooth.h"

#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include <furi_hal_bt.h>
#include <furi_hal_usb_hid.h>
#include <storage/storage.h>

#define TRANSPORT_BLUETOOTH_NVM_MS 200

struct Transport {
    Bt* bt;
    FuriHalBleProfileTemplate profile_template;
    FuriHalBleProfileBase* profile;
};

static const uint16_t transport_bluetooth_keys[TransportKeyMAX] = {
    [TransportKeyArrowDown] = HID_KEYBOARD_DOWN_ARROW,
    [TransportKeyArrowUp] = HID_KEYBOARD_UP_ARROW,
    [TransportKeyControlF] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_F,
    [TransportKeyControlL] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_L,
    [TransportKeyControlQ] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_Q,
    [TransportKeyControlX] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_X,
    [TransportKeyEnter] = HID_KEYBOARD_RETURN,
    [TransportKeyEsc] = HID_KEYBOARD_ESCAPE,
};

/* Neither 0, the stock Remote's address, nor 0x0002, BadUSB's. */
static BleProfileHidParams transport_bluetooth_params = {
    .mac_xor = 0xC1A0,
};

/* device_name_prefix stops under 8 characters (hid_profile.h), and byte 0 of adv_name is its AD type. */
static void transport_bluetooth_get_gap_config(GapConfig* config, FuriHalBleProfileParams params) {
    ble_profile_hid->get_gap_config(config, params);
    strlcpy(config->adv_name + 1, "Claude Remote", sizeof(config->adv_name) - 1);
}

/* Runs on the Bt thread, and event_loop.h binds its functions to the loop's own thread. */
static void transport_bluetooth_status_callback(BtStatus status, void* context) {
    FuriMessageQueue* link_queue = context;
    bool connected = status == BtStatusConnected;
    furi_message_queue_put(link_queue, &connected, 0);
}

Transport* transport_bluetooth_alloc(FuriMessageQueue* link_queue) {
    Transport* transport = malloc(sizeof(Transport));
    transport->bt = furi_record_open(RECORD_BT);
    transport->profile_template = *ble_profile_hid;
    transport->profile_template.get_gap_config = transport_bluetooth_get_gap_config;

    /* The stock order, hid.c:200-239: the second core writes its NVM after a disconnect. */
    bt_disconnect(transport->bt);
    furi_delay_ms(TRANSPORT_BLUETOOTH_NVM_MS);
    bt_keys_storage_set_storage_path(transport->bt, APP_DATA_PATH(".bt_hid.keys"));
    transport->profile =
        bt_profile_start(transport->bt, &transport->profile_template, &transport_bluetooth_params);
    furi_check(transport->profile);
    furi_hal_bt_start_advertising();
    bt_set_status_changed_callback(transport->bt, transport_bluetooth_status_callback, link_queue);

    return transport;
}

void transport_bluetooth_free(Transport* transport) {
    bt_set_status_changed_callback(transport->bt, NULL, NULL);
    bt_disconnect(transport->bt);
    furi_delay_ms(TRANSPORT_BLUETOOTH_NVM_MS);
    bt_keys_storage_set_default_path(transport->bt);
    furi_check(bt_profile_restore_default(transport->bt));
    furi_record_close(RECORD_BT);
    free(transport);
}

/* The result reads inverted (gatt.c:144, against hid_profile.h:26), so nothing branches on it. */
void transport_press(Transport* transport, TransportKey key) {
    ble_profile_hid_kb_press(transport->profile, transport_bluetooth_keys[key]);
}

void transport_release(Transport* transport, TransportKey key) {
    ble_profile_hid_kb_release(transport->profile, transport_bluetooth_keys[key]);
}
