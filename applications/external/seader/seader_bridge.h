#pragma once

#include <stdlib.h> // malloc
#include <stdint.h> // uint32_t
#include <stdarg.h> // __VA_ARGS__
#include <string.h>
#include <stdio.h>

#include <furi.h>
#include <furi_hal.h>

// https://ww1.microchip.com/downloads/en/DeviceDoc/00001561C.pdf
#define SEADER_UART_RX_BUF_SIZE (300)
#define SEADER_CCID_SLOT_COUNT  (2U)

typedef struct BitBuffer BitBuffer;

typedef struct {
    uint8_t uart_ch;
    uint8_t flow_pins;
    uint8_t baudrate_mode;
    uint32_t baudrate;
} SeaderUartConfig;

typedef struct {
    uint8_t protocol;
} SeaderUartState;

typedef struct {
    bool powered;
    /* CCID sequence counter for this slot. */
    uint8_t sequence;
} SeaderCcidSlotState;

typedef struct {
    bool has_sam;
    uint8_t sam_slot;
    uint8_t retries;
    SeaderCcidSlotState slots[SEADER_CCID_SLOT_COUNT];
} SeaderCcidState;

typedef struct {
    /* ICC information field size for T=1. */
    uint8_t ifsc;
    /* Host NAD used for T=1 block exchange. */
    uint8_t nad;
    /* Last transmit I-block sequence bit. */
    uint8_t send_pcb;
    /* Last receive I-block sequence bit. */
    uint8_t recv_pcb;
    BitBuffer* tx_buffer;
    size_t tx_buffer_offset;
    BitBuffer* rx_buffer;
} SeaderT1State;

struct SeaderUartBridge {
    SeaderUartConfig cfg;
    SeaderUartConfig cfg_new;

    FuriThread* thread;
    FuriThread* tx_thread;

    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;

    FuriSemaphore* tx_sem;

    SeaderUartState st;

    uint8_t rx_buf[SEADER_UART_RX_BUF_SIZE];
    uint8_t tx_buf[SEADER_UART_RX_BUF_SIZE];
    size_t tx_len;

    // T=0 or T=1
    uint8_t T;
    SeaderCcidState ccid;
    SeaderT1State t1;
};

typedef struct SeaderUartBridge SeaderUartBridge;
