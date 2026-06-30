#pragma once

#include "furi_hal.h"

#define RX_BUF_SIZE (320)

typedef struct BlackhatUart BlackhatUart;

void blackhat_uart_set_handle_rx_data_cb(
    BlackhatUart* uart,
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context)
);
void blackhat_uart_tx(BlackhatUart* uart, char* data, size_t len);
BlackhatUart* blackhat_uart_init(BlackhatApp* app);
void blackhat_uart_free(BlackhatUart* uart);
