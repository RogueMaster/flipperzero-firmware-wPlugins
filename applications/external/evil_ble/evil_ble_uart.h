#pragma once

#include <furi.h>
#include <furi_hal.h>

/* Opaque UART context for ESP32 WiFi Dev Board (Marauder) communication. */
typedef struct EvilBleUart EvilBleUart;

/* Callback invoked on the worker thread for each complete line received.
 * `line` is null-terminated, stripped of CR/LF, and valid only for the
 * duration of the call — copy if needed. */
typedef void (*EvilBleUartRxCallback)(const char* line, void* ctx);

/* Allocate and initialise the UART layer.
 * Acquires USART, disables the expansion module, and starts the RX worker.
 * Always returns a valid pointer; check evil_ble_uart_is_connected() to
 * determine whether the serial port was successfully acquired. */
EvilBleUart* evil_ble_uart_alloc(void);

/* Stop the worker thread, release the serial port, re-enable the expansion
 * module, and free all resources. */
void evil_ble_uart_free(EvilBleUart* uart);

/* Transmit `cmd` followed by a newline.  No-op if serial was not acquired. */
void evil_ble_uart_send(EvilBleUart* uart, const char* cmd);

/* Register a callback that fires for every complete line received.
 * Pointer-sized assignment — atomic on Cortex-M4, no mutex needed. */
void evil_ble_uart_set_rx_callback(EvilBleUart* uart, EvilBleUartRxCallback cb, void* ctx);

/* Returns true once the first line of data has been received from the board. */
bool evil_ble_uart_is_connected(EvilBleUart* uart);
