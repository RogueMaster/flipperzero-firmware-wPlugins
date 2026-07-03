/**
 * rogue_uart.c — UART communication layer for Rogue AP Detector.
 *
 * Architecture
 * ~~~~~~~~~~~~
 *   ISR rx_callback  →  FuriStreamBuffer  →  rx_worker thread  →
 * RogueUartRxCallback
 *
 * The ISR pushes raw bytes into the stream buffer with zero timeout so it
 * never blocks.  The worker thread accumulates bytes into a line buffer and
 * invokes the registered callback when it sees a newline.
 *
 * Adapted from flipperpwn/wifi_uart.c — same pattern, different namespace.
 */

#include "rogue_uart.h"

#include <expansion/expansion.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <stdlib.h>
#include <string.h>

#define TAG "RogueUart"

#define ROGUE_UART_RX_BUF_SIZE  1024
#define ROGUE_UART_LINE_BUF_LEN 512

/* --------------------------------------------------------------------------
 * Internal struct
 * -------------------------------------------------------------------------- */
struct RogueUart {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* rx_thread;
    RogueUartRxCallback rx_callback;
    void* rx_callback_ctx;
    Expansion* expansion;
    volatile bool connected;
    volatile bool running;
};

/* --------------------------------------------------------------------------
 * ISR — pushes raw bytes into the stream buffer.
 * Called from interrupt context; must not block.
 * -------------------------------------------------------------------------- */
static void
    rogue_uart_isr_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    RogueUart* uart = (RogueUart*)context;

    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        /* Zero timeout — ISR must never block. */
        furi_stream_buffer_send(uart->rx_stream, &byte, 1, 0);
    }
}

/* --------------------------------------------------------------------------
 * Worker thread — assembles bytes into lines, fires callback.
 * -------------------------------------------------------------------------- */
static int32_t rogue_uart_rx_worker(void* context) {
    RogueUart* uart = (RogueUart*)context;

    char line_buf[ROGUE_UART_LINE_BUF_LEN];
    size_t line_pos = 0;

    FURI_LOG_I(TAG, "UART worker started");

    while(uart->running) {
        uint8_t byte;
        size_t received = furi_stream_buffer_receive(
            uart->rx_stream,
            &byte,
            1,
            furi_ms_to_ticks(100) /* 100 ms timeout keeps loop responsive */
        );

        if(received == 0) {
            /* Timeout — re-check running flag. */
            continue;
        }

        /* Skip bare carriage returns; treat \r\n as a single newline. */
        if(byte == '\r') continue;

        if(byte == '\n') {
            line_buf[line_pos] = '\0';

            /* Skip empty lines and Marauder binary PCAP framing tokens. */
            if(line_pos > 0 && strncmp(line_buf, "[BUF/", 5) != 0) {
                if(!uart->connected) {
                    uart->connected = true;
                    FURI_LOG_I(TAG, "WiFi board connected");
                }

                if(uart->rx_callback) {
                    uart->rx_callback(line_buf, uart->rx_callback_ctx);
                }
            }

            line_pos = 0;
        } else {
            /* Accumulate — clamp at buffer limit to avoid stalling the
       * assembler on pathologically long lines. */
            if(line_pos < ROGUE_UART_LINE_BUF_LEN - 1) {
                line_buf[line_pos++] = (char)byte;
            }
        }
    }

    FURI_LOG_I(TAG, "UART worker stopped");
    return 0;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

RogueUart* rogue_uart_alloc(void) {
    RogueUart* uart = malloc(sizeof(RogueUart));
    furi_assert(uart);
    memset(uart, 0, sizeof(RogueUart));

    /* Disable the expansion module so it does not fight us for the USART. */
    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    uart->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!uart->serial) {
        FURI_LOG_W(TAG, "Failed to acquire USART — WiFi board not present");
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        uart->expansion = NULL;
        return uart;
    }

    furi_hal_serial_init(uart->serial, 115200);

    /* Stream buffer: 1024-byte capacity, trigger level 1 byte. */
    uart->rx_stream = furi_stream_buffer_alloc(ROGUE_UART_RX_BUF_SIZE, 1);
    furi_assert(uart->rx_stream);

    /* Start the worker before enabling async RX to avoid dropping bytes. */
    uart->running = true;
    uart->rx_thread = furi_thread_alloc_ex("RogueUartRx", 2048, rogue_uart_rx_worker, uart);
    furi_assert(uart->rx_thread);
    furi_thread_start(uart->rx_thread);

    furi_hal_serial_async_rx_start(uart->serial, rogue_uart_isr_rx_cb, uart, false);

    FURI_LOG_I(TAG, "UART initialised at 115200");
    return uart;
}

void rogue_uart_free(RogueUart* uart) {
    furi_assert(uart);

    if(uart->serial) {
        uart->running = false;
        furi_thread_join(uart->rx_thread);
        furi_thread_free(uart->rx_thread);
        uart->rx_thread = NULL;

        furi_hal_serial_async_rx_stop(uart->serial);
        furi_hal_serial_deinit(uart->serial);
        furi_hal_serial_control_release(uart->serial);
        uart->serial = NULL;

        furi_stream_buffer_free(uart->rx_stream);
        uart->rx_stream = NULL;
    }

    if(uart->expansion) {
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        uart->expansion = NULL;
    }

    free(uart);
}

void rogue_uart_send(RogueUart* uart, const char* cmd) {
    furi_assert(uart);

    if(!uart->serial || !cmd) return;

    size_t len = strlen(cmd);
    furi_hal_serial_tx(uart->serial, (const uint8_t*)cmd, len);

    const uint8_t nl = '\n';
    furi_hal_serial_tx(uart->serial, &nl, 1);

    furi_hal_serial_tx_wait_complete(uart->serial);

    FURI_LOG_D(TAG, "TX: %s", cmd);
}

void rogue_uart_set_rx_callback(RogueUart* uart, RogueUartRxCallback cb, void* ctx) {
    furi_assert(uart);
    uart->rx_callback_ctx = ctx;
    uart->rx_callback = cb;
}

bool rogue_uart_is_connected(RogueUart* uart) {
    furi_assert(uart);
    return uart->connected;
}
