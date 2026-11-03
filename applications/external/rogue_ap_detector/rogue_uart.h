#pragma once

/**
 * rogue_uart.h -- Thin shim over common/esp_serial for Rogue AP Detector.
 */

#include "../common/esp_serial.h"

typedef EspSerial RogueUart;
typedef EspSerialRxCallback RogueUartRxCallback;

static inline RogueUart* rogue_uart_alloc(void) {
    static const EspSerialConfig cfg = {
        .tag = "RogueUart",
        .thread_name = "RogueUartRx",
    };
    return esp_serial_alloc(&cfg);
}

#define rogue_uart_free esp_serial_free
#define rogue_uart_send esp_serial_send
#define rogue_uart_set_rx_callback esp_serial_set_rx_callback
#define rogue_uart_is_connected esp_serial_is_connected
