/*
 * Based on the BLE serial helper used by flipper-pc-monitor.
 * This bypasses the stock profile implementation and talks directly
 * to the serial BLE service so RX callbacks reach the app reliably.
 */

#include "ble_serial.h"

#include <ble/core/ble_defs.h>
#include <furi.h>
#include <gap.h>

typedef struct {
    FuriHalBleProfileBase base;
    BleServiceSerial* serial_svc;
} BleProfileSerial;
_Static_assert(offsetof(BleProfileSerial, base) == 0, "Wrong layout");

static FuriHalBleProfileBase* ble_profile_serial_start(FuriHalBleProfileParams profile_params) {
    UNUSED(profile_params);

    BleProfileSerial* profile = malloc(sizeof(BleProfileSerial));
    profile->base.config = ble_profile_serial;
    profile->serial_svc = ble_svc_serial_start();

    return &profile->base;
}

static void ble_profile_serial_stop(FuriHalBleProfileBase* profile) {
    furi_check(profile);
    furi_check(profile->config == ble_profile_serial);

    BleProfileSerial* serial_profile = (BleProfileSerial*)profile;
    ble_svc_serial_stop(serial_profile->serial_svc);
}

#define CONNECTION_INTERVAL_MIN (0x06)
#define CONNECTION_INTERVAL_MAX (0x24)

static const GapConfig serial_template_config = {
    .adv_service =
        {
            .UUID_Type = UUID_TYPE_16,
            .Service_UUID_16 = 0x3080,
        },
    .appearance_char = 0x8600,
    .bonding_mode = true,
    .pairing_method = GapPairingPinCodeShow,
    .conn_param =
        {
            .conn_int_min = CONNECTION_INTERVAL_MIN,
            .conn_int_max = CONNECTION_INTERVAL_MAX,
            .slave_latency = 0,
            .supervisor_timeout = 0,
        },
};

static void
    ble_profile_serial_get_config(GapConfig* config, FuriHalBleProfileParams profile_params) {
    BleProfileSerialParams* serial_profile_params = profile_params;

    furi_check(config);
    memcpy(config, &serial_template_config, sizeof(GapConfig));
    memcpy(config->mac_address, furi_hal_version_get_ble_mac(), sizeof(config->mac_address));

    config->mac_address[2]++;
    if(serial_profile_params) {
        config->mac_address[0] ^= serial_profile_params->mac_xor;
        config->mac_address[1] ^= serial_profile_params->mac_xor >> 8;
    }

    const char* name_prefix = "Serial";
    if(serial_profile_params && serial_profile_params->device_name_prefix) {
        name_prefix = serial_profile_params->device_name_prefix;
    }

    snprintf(
        config->adv_name,
        sizeof(config->adv_name),
        "%c%s %s",
        furi_hal_version_get_ble_local_device_name_ptr()[0],
        name_prefix,
        furi_hal_version_get_name_ptr());

    config->adv_service.UUID_Type = UUID_TYPE_16;
    config->adv_service.Service_UUID_16 |= furi_hal_version_get_hw_color();
}

static const FuriHalBleProfileTemplate profile_callbacks = {
    .start = ble_profile_serial_start,
    .stop = ble_profile_serial_stop,
    .get_gap_config = ble_profile_serial_get_config,
};

const FuriHalBleProfileTemplate* const ble_profile_serial = &profile_callbacks;

void ble_profile_serial_set_event_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buff_size,
    FuriHalBtSerialCallback callback,
    void* context) {
    furi_check(profile);
    furi_check(profile->config == ble_profile_serial);

    BleProfileSerial* serial_profile = (BleProfileSerial*)profile;
    ble_svc_serial_set_callbacks(serial_profile->serial_svc, buff_size, callback, context);
}

bool ble_profile_serial_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size) {
    furi_check(profile);
    furi_check(profile->config == ble_profile_serial);

    if(size > BLE_PROFILE_SERIAL_PACKET_SIZE_MAX) {
        return false;
    }

    BleProfileSerial* serial_profile = (BleProfileSerial*)profile;
    return ble_svc_serial_update_tx(serial_profile->serial_svc, data, size);
}

void ble_profile_serial_notify_buffer_is_empty(FuriHalBleProfileBase* profile) {
    furi_check(profile);
    furi_check(profile->config == ble_profile_serial);

    BleProfileSerial* serial_profile = (BleProfileSerial*)profile;
    ble_svc_serial_notify_buffer_is_empty(serial_profile->serial_svc);
}
