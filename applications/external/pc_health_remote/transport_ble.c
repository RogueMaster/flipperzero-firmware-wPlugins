// BLE transport: own profile template (serial GATT service, Just Works pairing + bonding,
// "PCHealth" advertised name, MAC distinct from the stock Flipper) on top of furi_ble.
#include <stdlib.h>
#include <string.h>
#include <furi.h>
#include <furi_hal.h>
#include <bt/bt_service/bt.h>
#include <furi_ble/profile_interface.h>
#include <services/serial_service.h>
#include "transport.h"
#include "settings.h"

#define TAG                 "PhrBle"
#define BLE_RX_BUFFER       256
#define BLE_SWITCH_DELAY_MS 200

#ifndef UUID_TYPE_16
#define UUID_TYPE_16 \
    0x01U // ble_defs.h value, not exported through the SDK headers on all firmwares
#endif

#ifndef AD_TYPE_COMPLETE_LOCAL_NAME
#define AD_TYPE_COMPLETE_LOCAL_NAME 0x09
#endif

/* ---------- profile template ---------- */

typedef struct {
    FuriHalBleProfileBase base;
    BleServiceSerial* serial;
} PhrBleProfile;

static const FuriHalBleProfileTemplate phr_ble_template;

static FuriHalBleProfileBase* phr_ble_profile_start(FuriHalBleProfileParams params) {
    UNUSED(params);
    PhrBleProfile* p = malloc(sizeof(PhrBleProfile));
    memset(p, 0, sizeof(PhrBleProfile));
    p->base.config = &phr_ble_template;
    p->serial = ble_svc_serial_start();
    return &p->base;
}

static void phr_ble_profile_stop(FuriHalBleProfileBase* base) {
    PhrBleProfile* p = (PhrBleProfile*)base;
    ble_svc_serial_stop(p->serial);
    free(p);
}

static void phr_ble_profile_get_config(GapConfig* config, FuriHalBleProfileParams params) {
    UNUSED(params);
    furi_check(config);
    memset(config, 0, sizeof(GapConfig));

    // Advertise the serial service, like the stock serial profile.
    config->adv_service.UUID_Type = UUID_TYPE_16;
    config->adv_service.Service_UUID_16 = 0x3080;
    config->appearance_char = 0x8600;

    // The serial service characteristics require an authenticated (MITM-protected) link,
    // so Just Works is not enough: writes would be silently dropped. Use numeric
    // comparison: the Flipper asks once to confirm the PIN, the PC backend accepts it
    // without any Windows UI. The bond is kept in the app's own key file, so every
    // later reconnect is silent.
    config->bonding_mode = true;
    config->pairing_method = GapPairingPinCodeVerifyYesNo;

    config->conn_param.conn_int_min = 0x18; // 30 ms
    config->conn_param.conn_int_max = 0x24; // 45 ms
    config->conn_param.slave_latency = 0;
    config->conn_param.supervisor_timeout = 0;

    // MAC: stock address with the first byte xor-ed, so the bond never clashes with
    // the mobile app / other profiles.
    memcpy(config->mac_address, furi_hal_version_get_ble_mac(), sizeof(config->mac_address));
    config->mac_address[0] ^= 0xA5;

    // Advertised name: "PCHealth <flipper name>" (first byte is the AD type).
    const char* name = furi_hal_version_get_name_ptr();
    snprintf(
        config->adv_name,
        sizeof(config->adv_name),
        "%cPCHealth %s",
        AD_TYPE_COMPLETE_LOCAL_NAME,
        (name && name[0]) ? name : "Zero");
}

static const FuriHalBleProfileTemplate phr_ble_template = {
    .start = phr_ble_profile_start,
    .stop = phr_ble_profile_stop,
    .get_gap_config = phr_ble_profile_get_config,
};

/* ---------- transport ---------- */

struct PhrTransport {
    PhrLink* link;
    Bt* bt;
    PhrBleProfile* profile;
    BtStatus status;
    char name[24];
};

static uint16_t phr_ble_serial_cb(SerialServiceEvent event, void* context) {
    PhrTransport* t = context;
    if(event.event == SerialServiceEventTypeDataReceived) {
        phr_link_feed(t->link, event.data.buffer, event.data.size);
    }
    return BLE_RX_BUFFER; // data is consumed synchronously
}

static void phr_ble_status_cb(BtStatus status, void* context) {
    PhrTransport* t = context;
    t->status = status;
    phr_link_set_connected(t->link, status == BtStatusConnected);
}

static PhrTransport* phr_ble_start(PhrLink* link) {
    PhrTransport* t = malloc(sizeof(PhrTransport));
    memset(t, 0, sizeof(PhrTransport));
    t->link = link;
    t->status = BtStatusOff;

    const char* dn = furi_hal_version_get_name_ptr();
    snprintf(t->name, sizeof(t->name), "PCHealth %s", (dn && dn[0]) ? dn : "Zero");

    t->bt = furi_record_open(RECORD_BT);
    // Drop any existing connection before switching the profile.
    bt_disconnect(t->bt);
    furi_delay_ms(BLE_SWITCH_DELAY_MS);
    // App-private bonding keys, so pairing never touches the user's phone bond.
    bt_keys_storage_set_storage_path(t->bt, APP_DATA_PATH(".bt.keys"));

    t->profile = (PhrBleProfile*)bt_profile_start(t->bt, &phr_ble_template, NULL);
    if(!t->profile) {
        FURI_LOG_E(TAG, "Profile start failed");
        bt_keys_storage_set_default_path(t->bt);
        bt_profile_restore_default(t->bt);
        furi_record_close(RECORD_BT);
        free(t);
        return NULL;
    }
    ble_svc_serial_set_callbacks(t->profile->serial, BLE_RX_BUFFER, phr_ble_serial_cb, t);
    bt_set_status_changed_callback(t->bt, phr_ble_status_cb, t);
    furi_hal_bt_start_advertising();
    t->status = BtStatusAdvertising;
    return t;
}

static void phr_ble_stop(PhrTransport* t) {
    bt_set_status_changed_callback(t->bt, NULL, NULL);
    bt_disconnect(t->bt);
    furi_delay_ms(BLE_SWITCH_DELAY_MS);
    ble_svc_serial_set_callbacks(t->profile->serial, 0, NULL, NULL);
    bt_keys_storage_set_default_path(t->bt);
    if(!bt_profile_restore_default(t->bt)) FURI_LOG_E(TAG, "Restore default profile failed");
    furi_record_close(RECORD_BT);
    phr_link_set_connected(t->link, false);
    free(t);
}

static void phr_ble_tick(PhrTransport* t) {
    if(t->status != BtStatusConnected) return;
    if(phr_link_hello_due(t->link)) {
        uint8_t buf[PHR_HELLO_LEN];
        size_t n = phr_link_build_hello(t->link, 0, buf);
        ble_svc_serial_update_tx(t->profile->serial, buf, (uint16_t)n);
    }
}

static const char* phr_ble_status(PhrTransport* t) {
    switch(t->status) {
    case BtStatusConnected:
        return "Connected";
    case BtStatusAdvertising:
        return "Waiting for PC";
    case BtStatusOff:
        return "Bluetooth off";
    default:
        return "Bluetooth unavailable";
    }
}

static const char* phr_ble_device_name(PhrTransport* t) {
    return t->name;
}

const PhrTransportApi phr_transport_ble = {
    .name = "Bluetooth LE",
    .wire_id = 0,
    .start = phr_ble_start,
    .stop = phr_ble_stop,
    .tick = phr_ble_tick,
    .status = phr_ble_status,
    .device_name = phr_ble_device_name,
};
