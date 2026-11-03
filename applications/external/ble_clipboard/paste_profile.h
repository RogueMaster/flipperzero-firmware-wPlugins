#pragma once

#include <furi_ble/profile_interface.h>
#include <services/serial_service.h>

extern const FuriHalBleProfileTemplate* ble_profile_paste;

void ble_profile_paste_set_serial_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buffer_size,
    SerialServiceEventCallback callback,
    void* context);

bool ble_profile_paste_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size);
