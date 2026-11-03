#pragma once

/**
 * evil_ble_uart.h -- Thin shim over common/esp_serial for Evil BLE.
 */

#include "../common/esp_serial.h"

typedef EspSerial EvilBleUart;
typedef EspSerialRxCallback EvilBleUartRxCallback;

static inline EvilBleUart* evil_ble_uart_alloc(void) {
    static const EspSerialConfig cfg = {
        .tag = "EvilBLE",
        .thread_name = "EvilBleUartRx",
    };
    return esp_serial_alloc(&cfg);
}

#define evil_ble_uart_free esp_serial_free
#define evil_ble_uart_send esp_serial_send
#define evil_ble_uart_set_rx_callback esp_serial_set_rx_callback
#define evil_ble_uart_is_connected esp_serial_is_connected
