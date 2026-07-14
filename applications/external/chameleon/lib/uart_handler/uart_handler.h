#pragma once

#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// UART configuration for USB communication
#define UART_BAUD_RATE 115200
#define UART_RX_BUFFER_SIZE 1024

// UART handler instance
typedef struct UartHandler UartHandler;

// Callback for received data
typedef void (*UartHandlerRxCallback)(const uint8_t* data, size_t length, void* context);

// Create and destroy UART handler
UartHandler* uart_handler_alloc();
void uart_handler_free(UartHandler* handler);

// Initialize UART (USB CDC)
bool uart_handler_init(UartHandler* handler);
void uart_handler_deinit(UartHandler* handler);

// Set receive callback
void uart_handler_set_rx_callback(UartHandler* handler, UartHandlerRxCallback callback, void* context);

// Send data
bool uart_handler_send(UartHandler* handler, const uint8_t* data, size_t length);

// Check connection status
bool uart_handler_is_connected(UartHandler* handler);

// Start/stop receiving
void uart_handler_start_rx(UartHandler* handler);
void uart_handler_stop_rx(UartHandler* handler);
