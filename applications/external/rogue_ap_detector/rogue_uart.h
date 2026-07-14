#pragma once

#include <furi.h>
#include <furi_hal.h>

/* Opaque UART context for WiFi Dev Board (Marauder) communication. */
typedef struct RogueUart RogueUart;

/* Callback invoked on the worker thread for each complete line received.
 * `line` is null-terminated, stripped of CR/LF, and valid only for the
 * duration of the call — copy if you need to retain it. */
typedef void (*RogueUartRxCallback)(const char* line, void* ctx);

/* Allocate and initialise the UART layer.
 * Acquires USART, disables the expansion module, starts the RX worker.
 * Always returns a valid pointer; the serial handle may be NULL if no
 * ESP32 is present — callers should handle that gracefully. */
RogueUart* rogue_uart_alloc(void);

/* Stop the worker thread, release the serial port, re-enable the expansion
 * module, and free all resources. */
void rogue_uart_free(RogueUart* uart);

/* Transmit `cmd` followed by a newline.  No-op if serial was not acquired. */
void rogue_uart_send(RogueUart* uart, const char* cmd);

/* Register a callback that fires for every complete line received.
 * Pointer-sized assignment — atomic on Cortex-M4, no mutex needed. */
void rogue_uart_set_rx_callback(RogueUart* uart, RogueUartRxCallback cb, void* ctx);

/* Returns true once the first line of data has been received from the board. */
bool rogue_uart_is_connected(RogueUart* uart);
