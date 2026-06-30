#pragma once

#include <stdlib.h> // malloc
#include <stdint.h> // uint32_t
#include <stdarg.h> // __VA_ARGS__
#include <string.h>
#include <stdio.h>

#include <furi.h>
#include <furi_hal.h>

#define SEOS_UART_RX_BUF_SIZE (256)

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone | WorkerEvtDevTxComplete)
#define WORKER_ALL_TX_EVENTS (WorkerEvtTxStop | WorkerEvtDevRx)

typedef size_t (*SeosUartReceiveCallback)(void* context, uint8_t* buffer, size_t len);

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),

    WorkerEvtTxStop = (1 << 2),
    WorkerEvtDevRx = (1 << 3),
    WorkerEvtDevTxComplete = (1 << 4),
} WorkerEvtFlags;

typedef struct {
    uint8_t uart_ch;
    uint8_t flow_pins;
    uint8_t baudrate_mode;
    uint32_t baudrate;
} SeosUartConfig;

struct SeosUart {
    SeosUartConfig cfg;
    SeosUartConfig cfg_new;

    FuriThread* thread;
    FuriThread* tx_thread;

    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;

    FuriSemaphore* tx_sem;

    uint8_t rx_buf[SEOS_UART_RX_BUF_SIZE];
    uint8_t tx_buf[SEOS_UART_RX_BUF_SIZE];
    size_t tx_len;

    SeosUartReceiveCallback receive_callback;
    void* receive_callback_context;
};

typedef struct SeosUart SeosUart;
