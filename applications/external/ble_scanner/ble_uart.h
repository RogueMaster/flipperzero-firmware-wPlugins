#pragma once

/**
 * ble_uart.h -- Thin shim over common/esp_serial for BLE Scanner.
 *
 * Preserves the app-specific type names and API so callers are unchanged.
 */

#include "../common/esp_serial.h"

typedef EspSerial BleUart;
typedef EspSerialRxCallback BleUartRxCallback;

static inline BleUart* ble_uart_alloc(void) {
    static const EspSerialConfig cfg = {
        .tag = "BleUart",
        .thread_name = "BleUartRx",
    };
    return esp_serial_alloc(&cfg);
}

#define ble_uart_free esp_serial_free
#define ble_uart_send esp_serial_send
#define ble_uart_set_rx_callback esp_serial_set_rx_callback
#define ble_uart_is_connected esp_serial_is_connected
