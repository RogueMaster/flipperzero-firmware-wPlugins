#pragma once

#include <furi.h>
#include <furi_hal.h>

/* Opaque UART context for WiFi Dev Board communication. */
typedef struct FPwnWifiUart FPwnWifiUart;

/* Callback invoked on the worker thread for each complete line received.
 * `line` is null-terminated, stripped of CR/LF, and valid only for the
 * duration of the call — copy if you need to retain it. */
typedef void (*FPwnWifiRxCallback)(const char* line, void* ctx);

/* Allocate and initialise the UART layer.
 * Acquires USART, disables the expansion module, and starts the RX worker.
 * Always returns a valid pointer; check fpwn_wifi_uart_is_connected() to
 * determine whether the serial port was successfully acquired. */
FPwnWifiUart* fpwn_wifi_uart_alloc(void);

/* Stop the worker thread, release the serial port, re-enable the expansion
 * module, and free all resources. */
void fpwn_wifi_uart_free(FPwnWifiUart* uart);

/* Transmit `cmd` followed by a newline.  No-op if serial was not acquired. */
void fpwn_wifi_uart_send(FPwnWifiUart* uart, const char* cmd);

/* Register a callback that fires for every complete line received.
 * Thread-safe; may be called before or after alloc. */
void fpwn_wifi_uart_set_rx_callback(FPwnWifiUart* uart, FPwnWifiRxCallback cb, void* ctx);

/* Returns true once the first line of data has been received from the board. */
bool fpwn_wifi_uart_is_connected(FPwnWifiUart* uart);
