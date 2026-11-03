#pragma once

#include <services/serial_service.h>

typedef struct BleServiceClipboard BleServiceClipboard;

BleServiceClipboard* ble_svc_clipboard_start(void);
void ble_svc_clipboard_stop(BleServiceClipboard* service);
void ble_svc_clipboard_set_callback(
    BleServiceClipboard* service,
    SerialServiceEventCallback callback,
    void* context);
bool ble_svc_clipboard_tx(BleServiceClipboard* service, const uint8_t* data, uint16_t size);
