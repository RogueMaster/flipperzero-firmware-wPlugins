#include "paste_profile.h"
#include "clipboard_service.h"

#include <furi.h>
#include <services/battery_service.h>
#include <services/dev_info_service.h>
#include <ble/core/ble_defs.h>

typedef struct {
    FuriHalBleProfileBase base;
    BleServiceBattery* battery_service;
    BleServiceDevInfo* device_info_service;
    BleServiceClipboard* clipboard_service;
} BleProfilePaste;

static FuriHalBleProfileBase* profile_start(FuriHalBleProfileParams params) {
    UNUSED(params);
    BleProfilePaste* profile = malloc(sizeof(BleProfilePaste));
    memset(profile, 0, sizeof(BleProfilePaste));
    profile->base.config = ble_profile_paste;
    profile->battery_service = ble_svc_battery_start(true);
    profile->device_info_service = ble_svc_dev_info_start();
    profile->clipboard_service = ble_svc_clipboard_start();
    if(!profile->clipboard_service) {
        ble_svc_dev_info_stop(profile->device_info_service);
        ble_svc_battery_stop(profile->battery_service);
        free(profile);
        return NULL;
    }
    return &profile->base;
}

static void profile_stop(FuriHalBleProfileBase* base) {
    furi_check(base && base->config == ble_profile_paste);
    BleProfilePaste* profile = (BleProfilePaste*)base;
    ble_svc_clipboard_stop(profile->clipboard_service);
    ble_svc_dev_info_stop(profile->device_info_service);
    ble_svc_battery_stop(profile->battery_service);
}

static void profile_get_config(GapConfig* config, FuriHalBleProfileParams params) {
    UNUSED(params);
    memset(config, 0, sizeof(GapConfig));
    config->adv_service.UUID_Type = UUID_TYPE_128;
    static const uint8_t uuid[] = {
        0x01,
        0x03,
        0xAA,
        0x10,
        0x77,
        0x3D,
        0x52,
        0x8E,
        0x9A,
        0x4C,
        0x21,
        0x7B,
        0xC0,
        0xD9,
        0xF1,
        0xA6,
    };
    memcpy(config->adv_service.Service_UUID_128, uuid, sizeof(uuid));
    config->bonding_mode = true;
    config->pairing_method = GapPairingPinCodeVerifyYesNo;
    config->conn_param.conn_int_min = 0x06;
    config->conn_param.conn_int_max = 0x24;
    memcpy(config->mac_address, furi_hal_version_get_ble_mac(), sizeof(config->mac_address));
    config->mac_address[2] += 3;
    config->mac_address[0] ^= 0x08;
    // Leave room for the full 128-bit service UUID in the advertisement.
    strlcpy(
        config->adv_name,
        "\x09"
        "Paste",
        sizeof(config->adv_name));
}

static const FuriHalBleProfileTemplate profile_template = {
    .start = profile_start,
    .stop = profile_stop,
    .get_gap_config = profile_get_config,
};

const FuriHalBleProfileTemplate* ble_profile_paste = &profile_template;

static BleProfilePaste* profile_cast(FuriHalBleProfileBase* base) {
    furi_check(base && base->config == ble_profile_paste);
    return (BleProfilePaste*)base;
}

void ble_profile_paste_set_serial_callback(
    FuriHalBleProfileBase* base,
    uint16_t buffer_size,
    SerialServiceEventCallback callback,
    void* context) {
    UNUSED(buffer_size);
    ble_svc_clipboard_set_callback(profile_cast(base)->clipboard_service, callback, context);
}

bool ble_profile_paste_tx(FuriHalBleProfileBase* base, uint8_t* data, uint16_t size) {
    return ble_svc_clipboard_tx(profile_cast(base)->clipboard_service, data, size);
}
