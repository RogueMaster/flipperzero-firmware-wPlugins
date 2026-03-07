#pragma once

/**
 * rayhunter_uart.h — UART layer for Ray Hunter Client.
 *
 * Self-contained copy of the ISR/worker pattern from FlipperPwn, adapted
 * for this app's naming and thread-stack requirements.
 *
 * Architecture:
 *   ISR rx_callback → FuriStreamBuffer → rx_worker thread → RhUartRxCallback
 */

#include <furi.h>
#include <furi_hal.h>

/** Opaque UART context. */
typedef struct RhUart RhUart;

/**
 * Callback invoked on the worker thread for each complete line received.
 * `line` is null-terminated, stripped of CR/LF, and valid only for the
 * duration of the call — copy if you need to retain it.
 */
typedef void (*RhUartRxCallback)(const char* line, void* ctx);

/**
 * Allocate and initialise the UART layer.
 * Acquires USART, disables the expansion module, and starts the RX worker.
 * Always returns a valid pointer — check rh_uart_is_connected() to determine
 * whether the serial port was successfully acquired.
 */
RhUart* rh_uart_alloc(void);

/**
 * Stop the worker thread, release the serial port, re-enable the expansion
 * module, and free all resources.
 */
void rh_uart_free(RhUart* uart);

/**
 * Transmit `cmd` followed by a newline.
 * No-op if serial was not acquired.
 */
void rh_uart_send(RhUart* uart, const char* cmd);

/**
 * Register a callback that fires for every complete line received.
 * Thread-safe; may be called before or after alloc.
 */
void rh_uart_set_rx_callback(RhUart* uart, RhUartRxCallback cb, void* ctx);

/** Returns true once the first line of data has been received from the board.
 */
bool rh_uart_is_connected(RhUart* uart);
