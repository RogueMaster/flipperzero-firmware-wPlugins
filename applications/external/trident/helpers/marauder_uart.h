#pragma once

#include <furi.h>
#include <stdbool.h>

/*
 * Serial link to an ESP32 Marauder companion board over the Flipper GPIO UART.
 *
 * The Marauder CLI is line-oriented ASCII at 115200 8N1. Trident sends commands
 * (with a trailing newline) and the board streams text back; a worker thread
 * reassembles that stream into lines and hands each one to a callback, which
 * the app forwards to the console view.
 *
 * The physical pins depend on the channel:
 *   USART  -> GPIO 13 (TX) / 14 (RX)  — the standard WiFi dev board wiring
 *   LPUART -> GPIO 15 (TX) / 16 (RX)  — dual-band / GPS "all-in-one" boards that
 *                                       keep 13/14 free for the GPS module
 */

typedef enum {
    MarauderUartChannelUsart = 0, // GPIO 13/14
    MarauderUartChannelLpuart = 1, // GPIO 15/16
} MarauderUartChannelId;

typedef struct MarauderUart MarauderUart;

// Called from the worker thread for every complete line received from the board.
typedef void (*MarauderUartLineCallback)(void* context, const char* line);

MarauderUart* marauder_uart_alloc(void);
void marauder_uart_free(MarauderUart* uart);

void marauder_uart_set_line_callback(
    MarauderUart* uart,
    MarauderUartLineCallback callback,
    void* context);

// Selects the physical channel. Takes effect on the next start(); ignored while running.
void marauder_uart_set_channel(MarauderUart* uart, MarauderUartChannelId channel);

void marauder_uart_start(MarauderUart* uart);
void marauder_uart_stop(MarauderUart* uart);
bool marauder_uart_is_running(MarauderUart* uart);

// Transmit raw bytes verbatim (caller supplies any trailing newline).
void marauder_uart_send(MarauderUart* uart, const char* data);
