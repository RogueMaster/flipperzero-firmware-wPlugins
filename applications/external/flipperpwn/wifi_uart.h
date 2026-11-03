#pragma once

/**
 * wifi_uart.h -- Thin shim over common/esp_serial for FlipperPwn.
 *
 * FlipperPwn sends a probe newline on open to trigger the ESP32 prompt.
 */

#include "../common/esp_serial.h"

typedef EspSerial FPwnWifiUart;
typedef EspSerialRxCallback FPwnWifiRxCallback;

static inline FPwnWifiUart* fpwn_wifi_uart_alloc(void) {
    static const EspSerialConfig cfg = {
        .tag = "FPwn",
        .thread_name = "FPwnUartRx",
        .probe_on_open = true,
    };
    return esp_serial_alloc(&cfg);
}

#define fpwn_wifi_uart_free esp_serial_free
#define fpwn_wifi_uart_send esp_serial_send
#define fpwn_wifi_uart_set_rx_callback esp_serial_set_rx_callback
#define fpwn_wifi_uart_is_connected esp_serial_is_connected
