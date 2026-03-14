/*
 * Based on the BLE serial helper used by flipper-pc-monitor.
 * This bypasses the stock profile implementation and talks directly
 * to the serial BLE service so RX callbacks reach the app reliably.
 */

#pragma once

#include <furi_ble/profile_interface.h>
#include <services/serial_service.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* device_name_prefix;
    uint16_t mac_xor;
} BleProfileSerialParams;

#define BLE_PROFILE_SERIAL_PACKET_SIZE_MAX BLE_SVC_SERIAL_DATA_LEN_MAX

typedef SerialServiceEventCallback FuriHalBtSerialCallback;

extern const FuriHalBleProfileTemplate* const ble_profile_serial;

bool ble_profile_serial_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size);

void ble_profile_serial_notify_buffer_is_empty(FuriHalBleProfileBase* profile);

void ble_profile_serial_set_event_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buff_size,
    FuriHalBtSerialCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
