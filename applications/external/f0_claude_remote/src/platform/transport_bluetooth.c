#include "transport_bluetooth.h"

#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include <furi_hal_bt.h>
#include <storage/storage.h>

#define TRANSPORT_BLUETOOTH_NVM_MS 200

struct TransportBluetooth {
    Bt* bt;
    FuriHalBleProfileTemplate profile_template;
    FuriHalBleProfileBase* profile;
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

TransportBluetooth* transport_bluetooth_alloc(FuriMessageQueue* link_queue) {
    TransportBluetooth* bluetooth = malloc(sizeof(TransportBluetooth));
    bluetooth->bt = furi_record_open(RECORD_BT);
    bluetooth->profile_template = *ble_profile_hid;
    bluetooth->profile_template.get_gap_config = transport_bluetooth_get_gap_config;

    /* The stock order, hid.c:200-239: the second core writes its NVM after a disconnect. */
    bt_disconnect(bluetooth->bt);
    furi_delay_ms(TRANSPORT_BLUETOOTH_NVM_MS);
    bt_keys_storage_set_storage_path(bluetooth->bt, APP_DATA_PATH(".bt_hid.keys"));
    bluetooth->profile =
        bt_profile_start(bluetooth->bt, &bluetooth->profile_template, &transport_bluetooth_params);
    furi_check(bluetooth->profile);
    furi_hal_bt_start_advertising();
    bt_set_status_changed_callback(bluetooth->bt, transport_bluetooth_status_callback, link_queue);

    return bluetooth;
}

void transport_bluetooth_free(TransportBluetooth* bluetooth) {
    bt_set_status_changed_callback(bluetooth->bt, NULL, NULL);
    bt_disconnect(bluetooth->bt);
    furi_delay_ms(TRANSPORT_BLUETOOTH_NVM_MS);
    bt_keys_storage_set_default_path(bluetooth->bt);
    furi_check(bt_profile_restore_default(bluetooth->bt));
    furi_record_close(RECORD_BT);
    free(bluetooth);
}

/* The result reads inverted (gatt.c:144, against hid_profile.h:26), so nothing branches on it. */
void transport_bluetooth_press(TransportBluetooth* bluetooth, uint16_t key) {
    ble_profile_hid_kb_press(bluetooth->profile, key);
}

void transport_bluetooth_release(TransportBluetooth* bluetooth, uint16_t key) {
    ble_profile_hid_kb_release(bluetooth->profile, key);
}

void transport_bluetooth_offer(TransportBluetooth* bluetooth) {
    UNUSED(bluetooth);
    furi_hal_bt_start_advertising();
}

void transport_bluetooth_withdraw(TransportBluetooth* bluetooth) {
    UNUSED(bluetooth);
    furi_hal_bt_stop_advertising();
}
