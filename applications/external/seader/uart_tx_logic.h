#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SEADER_UART_TX_FRAME_MAX_SIZE (272U)

typedef struct {
    size_t len;
    uint8_t data[SEADER_UART_TX_FRAME_MAX_SIZE];
} SeaderUartTxFrame;

bool seader_uart_tx_frame_copy(
    SeaderUartTxFrame* out,
    const uint8_t* data,
    size_t len,
    size_t max_len);
