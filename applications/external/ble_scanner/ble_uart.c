/**
 * ble_uart.c — UART communication layer for BLE Scanner (ESP32 Marauder).
 *
 * Architecture
 * ~~~~~~~~~~~~
 *   ISR rx_callback  →  FuriStreamBuffer  →  rx_worker thread  →
 * BleUartRxCallback
 *
 * The ISR pushes raw bytes into the stream buffer with zero timeout so it
 * never blocks.  The worker thread accumulates bytes into a line buffer and
 * invokes the registered callback when it sees a newline.
 *
 * Lifecycle
 * ~~~~~~~~~
 *   ble_uart_alloc()  — acquire serial, start worker
 *   ble_uart_send()   — transmit a command
 *   ble_uart_free()   — stop worker, release serial
 */

#include "ble_uart.h"

#include <expansion/expansion.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <stdlib.h>
#include <string.h>

#define TAG "BleUart"

/* Internal buffer sizes. */
#define BLE_UART_RX_BUF_SIZE  1024
#define BLE_UART_LINE_BUF_LEN 512

/* --------------------------------------------------------------------------
 * Internal struct
 * -------------------------------------------------------------------------- */
struct BleUart {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* rx_thread;
    BleUartRxCallback rx_callback;
    void* rx_callback_ctx;
    Expansion* expansion;
    volatile bool connected;
    volatile bool running;
};

/* --------------------------------------------------------------------------
 * ISR — pushes raw bytes into the stream buffer
 *
 * Called from interrupt context; must not block.
 * -------------------------------------------------------------------------- */
static void
    ble_uart_isr_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    BleUart* uart = (BleUart*)context;

    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        /* Zero timeout — ISR must never block. */
        furi_stream_buffer_send(uart->rx_stream, &byte, 1, 0);
    }
}

/* --------------------------------------------------------------------------
 * Worker thread — assembles bytes into lines, fires callback
 * -------------------------------------------------------------------------- */
static int32_t ble_uart_rx_worker(void* context) {
    BleUart* uart = (BleUart*)context;

    char line_buf[BLE_UART_LINE_BUF_LEN];
    size_t line_pos = 0;

    FURI_LOG_I(TAG, "UART worker started");

    while(uart->running) {
        uint8_t byte;
        size_t received = furi_stream_buffer_receive(
            uart->rx_stream,
            &byte,
            1,
            furi_ms_to_ticks(100) /* 100 ms timeout — keeps loop responsive */
        );

        if(received == 0) {
            /* Timeout, no data — loop and re-check running flag. */
            continue;
        }

        /* Skip bare carriage returns; handle \r\n as a single newline. */
        if(byte == '\r') continue;

        if(byte == '\n') {
            /* Null-terminate and dispatch the completed line. */
            line_buf[line_pos] = '\0';

            /* Skip empty lines and Marauder binary PCAP framing tokens. */
            if(line_pos > 0 && strncmp(line_buf, "[BUF/", 5) != 0) {
                /* First received line marks the board as alive. */
                if(!uart->connected) {
                    uart->connected = true;
                    FURI_LOG_I(TAG, "ESP32 board connected");
                }

                if(uart->rx_callback) {
                    uart->rx_callback(line_buf, uart->rx_callback_ctx);
                }
            }

            line_pos = 0;
        } else {
            /* Accumulate — guard against overflow by clamping. */
            if(line_pos < BLE_UART_LINE_BUF_LEN - 1) {
                line_buf[line_pos++] = (char)byte;
            }
            /* If the buffer is full we silently drop further bytes until the
       * next newline; this prevents a single very long line from
       * permanently stalling the line assembler. */
        }
    }

    FURI_LOG_I(TAG, "UART worker stopped");
    return 0;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

BleUart* ble_uart_alloc(void) {
    BleUart* uart = malloc(sizeof(BleUart));
    furi_assert(uart);
    memset(uart, 0, sizeof(BleUart));

    /* Disable the expansion module so it does not fight us for the USART. */
    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    /* Attempt to acquire the serial handle. */
    uart->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!uart->serial) {
        /* Board not connected or port busy — return an inert struct. */
        FURI_LOG_W(TAG, "Failed to acquire USART — ESP32 board not present");
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        uart->expansion = NULL;
        return uart;
    }

    furi_hal_serial_init(uart->serial, 115200);

    /* Stream buffer: 1024-byte capacity, trigger level 1 byte. */
    uart->rx_stream = furi_stream_buffer_alloc(BLE_UART_RX_BUF_SIZE, 1);
    furi_assert(uart->rx_stream);

    /* Start the line-assembler worker before enabling async RX so that no
   * bytes are dropped between the two operations. */
    uart->running = true;
    uart->rx_thread = furi_thread_alloc_ex("BleUartRx", 2048, ble_uart_rx_worker, uart);
    furi_assert(uart->rx_thread);
    furi_thread_start(uart->rx_thread);

    /* Hook up the ISR — bytes flow from here into rx_stream. */
    furi_hal_serial_async_rx_start(uart->serial, ble_uart_isr_rx_cb, uart, false);

    FURI_LOG_I(TAG, "UART initialised at 115200");
    return uart;
}

void ble_uart_free(BleUart* uart) {
    furi_assert(uart);

    if(uart->serial) {
        /* Signal the worker to exit and wait for it to drain. */
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

void ble_uart_send(BleUart* uart, const char* cmd) {
    furi_assert(uart);

    if(!uart->serial || !cmd) return;

    size_t len = strlen(cmd);
    furi_hal_serial_tx(uart->serial, (const uint8_t*)cmd, len);

    /* Marauder expects a newline to execute each command. */
    const uint8_t nl = '\n';
    furi_hal_serial_tx(uart->serial, &nl, 1);

    furi_hal_serial_tx_wait_complete(uart->serial);

    FURI_LOG_D(TAG, "TX: %s", cmd);
}

void ble_uart_set_rx_callback(BleUart* uart, BleUartRxCallback cb, void* ctx) {
    furi_assert(uart);
    /* Disable callback first to prevent the worker from invoking the old
     * callback with the new (possibly NULL) context during deregistration. */
    uart->rx_callback = NULL;
    __DMB();
    uart->rx_callback_ctx = ctx;
    __DMB();
    uart->rx_callback = cb;
}

bool ble_uart_is_connected(BleUart* uart) {
    furi_assert(uart);
    return uart->connected;
}
