#pragma once

/**
 * rayhunter_uart.h -- Thin shim over common/esp_serial for Ray Hunter Client.
 *
 * Uses a custom connected predicate: the board is "connected" only when a
 * line contains a Ray Hunter protocol marker (RH:, analysis_state, rayhunter).
 */

#include "../common/esp_serial.h"
#include <string.h>

typedef EspSerial RhUart;
typedef EspSerialRxCallback RhUartRxCallback;

static inline bool rh_uart_connected_check(const char* line) {
    return strstr(line, "RH:") || strstr(line, "analysis_state") ||
           strstr(line, "rayhunter");
}

static inline RhUart* rh_uart_alloc(void) {
    static const EspSerialConfig cfg = {
        .tag = "RhUart",
        .thread_name = "RhUartRx",
        .connected_check = rh_uart_connected_check,
    };
    return esp_serial_alloc(&cfg);
}

#define rh_uart_free esp_serial_free
#define rh_uart_send esp_serial_send
#define rh_uart_set_rx_callback esp_serial_set_rx_callback
#define rh_uart_is_connected esp_serial_is_connected
