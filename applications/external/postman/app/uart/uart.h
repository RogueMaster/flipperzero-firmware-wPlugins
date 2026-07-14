

#pragma once

#include <furi_hal.h>

typedef struct Uart Uart;

void uart_terminal_uart_set_handle_rx_data_cb(
    Uart* uart,
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context));
bool uart_terminal_uart_tx(Uart* uart, uint8_t* data, size_t len);
Uart* uart_terminal_uart_init(void* context);
void uart_terminal_uart_free(Uart* uart);
bool uart_terminal_uart_send_version_command(Uart* uart);
bool uart_terminal_uart_check_status(Uart* uart);

// Cmds
bool listWiFiCommand(Uart* uart, const char* argument);
bool activeWifiCommand(Uart* uart, const char* argument);
bool disconnectWiFiCommand(Uart* uart, const char* argument);
bool getCommand(Uart* uart, const char* argument);
bool getStreamCommand(Uart* uart, const char* argument);
bool postCommand(Uart* uart, const char* url_argument, FuriString* payload);
bool savePostToFileCommand(Uart* uart, const char* url_argument, FuriString* payload);
bool buildHttpMethodCommand(Uart* uart, const char* argument);
bool buildHttpUrlCommand(Uart* uart, const char* argument);
bool buildHttpHeaderCommand(Uart* uart, const char* argument);
bool buildHttpPayloadCommand(Uart* uart, const char* argument);
bool resetHttpConfigCommand(Uart* uart, const char* argument);
bool buildHttpShowResponseHeadersCommand(Uart* uart, const char* argument);
bool buildHttpImplementationCommand(Uart* uart, const char* argument);
bool executeHttpCallCommand(Uart* uart, const char* argument);
bool connectCommand(Uart* uart, const char* argument);
bool saveToFileCommand(Uart* uart, const char* argument);
