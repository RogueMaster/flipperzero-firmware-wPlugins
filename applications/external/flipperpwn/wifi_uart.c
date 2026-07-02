/**
 * wifi_uart.c — UART communication layer for WiFi Dev Board (Marauder).
 *
 * Architecture
 * ~~~~~~~~~~~~
 *   ISR rx_callback  →  FuriStreamBuffer  →  rx_worker thread  →  FPwnWifiRxCallback
 *
 * The ISR pushes raw bytes into the stream buffer with zero timeout so it
 * never blocks.  The worker thread accumulates bytes into a line buffer and
 * invokes the registered callback when it sees a newline.
 *
 * Lifecycle
 * ~~~~~~~~~
 *   fpwn_wifi_uart_alloc()  — acquire serial, start worker
 *   fpwn_wifi_uart_send()   — transmit a command
 *   fpwn_wifi_uart_free()   — stop worker, release serial
 */

#include "wifi_uart.h"

#include <expansion/expansion.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <string.h>
#include <stdlib.h>

#define TAG "FPwn"

/* Internal buffer sizes. */
#define FPWN_UART_RX_BUF_SIZE  1024
#define FPWN_UART_LINE_BUF_LEN 512

/* --------------------------------------------------------------------------
 * Internal struct
 * -------------------------------------------------------------------------- */
struct FPwnWifiUart {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* rx_thread;
    FPwnWifiRxCallback rx_callback;
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
    fpwn_uart_isr_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    FPwnWifiUart* uart = (FPwnWifiUart*)context;

    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        /* Zero timeout — ISR must never block. */
        furi_stream_buffer_send(uart->rx_stream, &byte, 1, 0);
    }
}

/* --------------------------------------------------------------------------
 * Worker thread — assembles bytes into lines, fires callback
 * -------------------------------------------------------------------------- */
static int32_t fpwn_uart_rx_worker(void* context) {
    FPwnWifiUart* uart = (FPwnWifiUart*)context;

    char line_buf[FPWN_UART_LINE_BUF_LEN];
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

        /* Skip bare carriage returns; we handle \r\n as a single newline. */
        if(byte == '\r') continue;

        if(byte == '\n') {
            /* Null-terminate and dispatch the completed line. */
            line_buf[line_pos] = '\0';

            if(line_pos > 0) {
                /* Any received line proves the ESP32 is alive, including
                 * the ">" prompt response to our probe newline. */
                if(!uart->connected) {
                    uart->connected = true;
                    FURI_LOG_I(TAG, "WiFi board connected");
                }
                /* Filter binary PCAP framing before dispatch. */
                if(strncmp(line_buf, "[BUF/", 5) != 0 && uart->rx_callback) {
                    uart->rx_callback(line_buf, uart->rx_callback_ctx);
                }
            }

            line_pos = 0;
        } else {
            /* Accumulate — guard against overflow by clamping. */
            if(line_pos < FPWN_UART_LINE_BUF_LEN - 1) {
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

FPwnWifiUart* fpwn_wifi_uart_alloc(void) {
    FPwnWifiUart* uart = malloc(sizeof(FPwnWifiUart));
    furi_assert(uart);
    memset(uart, 0, sizeof(FPwnWifiUart));

    /* Disable the expansion module so it does not fight us for the USART. */
    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    /* Attempt to acquire the serial handle. */
    uart->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!uart->serial) {
        /* Board not connected or port busy — return an inert struct. */
        FURI_LOG_W(TAG, "Failed to acquire USART — WiFi board not present");
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        uart->expansion = NULL;
        return uart;
    }

    furi_hal_serial_init(uart->serial, 115200);

    /* Stream buffer: 1024-byte capacity, trigger level 1 byte. */
    uart->rx_stream = furi_stream_buffer_alloc(FPWN_UART_RX_BUF_SIZE, 1);
    furi_assert(uart->rx_stream);

    /* Start the line-assembler worker before enabling async RX so that no
     * bytes are dropped between the two operations. */
    uart->running = true;
    uart->rx_thread = furi_thread_alloc_ex("FPwnUartRx", 2048, fpwn_uart_rx_worker, uart);
    furi_assert(uart->rx_thread);
    furi_thread_start(uart->rx_thread);

    /* Hook up the ISR — bytes flow from here into rx_stream. */
    furi_hal_serial_async_rx_start(uart->serial, fpwn_uart_isr_rx_cb, uart, false);

    /* Probe the ESP32 with an empty line to trigger a prompt response.
     * This sets uart->connected = true if the board is present. */
    const uint8_t nl = '\n';
    furi_hal_serial_tx(uart->serial, &nl, 1);
    furi_hal_serial_tx_wait_complete(uart->serial);

    FURI_LOG_I(TAG, "UART initialised at 115200");
    return uart;
}

void fpwn_wifi_uart_free(FPwnWifiUart* uart) {
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

void fpwn_wifi_uart_send(FPwnWifiUart* uart, const char* cmd) {
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

void fpwn_wifi_uart_set_rx_callback(FPwnWifiUart* uart, FPwnWifiRxCallback cb, void* ctx) {
    furi_assert(uart);
    /* Store context first, barrier, then function pointer.  The worker thread
     * tests rx_callback before calling it — this order guarantees that by the
     * time the worker sees the new function pointer, ctx is already visible. */
    uart->rx_callback_ctx = ctx;
    __DMB();
    uart->rx_callback = cb;
}

bool fpwn_wifi_uart_is_connected(FPwnWifiUart* uart) {
    furi_assert(uart);
    return uart->connected;
}
