/**
 * esp_serial.c -- Shared UART driver for ESP32/Marauder dev boards.
 *
 * Architecture:
 *   ISR rx_callback -> FuriStreamBuffer -> rx_worker thread -> EspSerialRxCallback
 *
 * The ISR pushes raw bytes into the stream buffer with zero timeout so it
 * never blocks.  The worker thread accumulates bytes into a line buffer and
 * invokes the registered callback when it sees a newline.
 */

#include "esp_serial.h"

#include <expansion/expansion.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <stdlib.h>
#include <string.h>

#define ESP_SERIAL_RX_BUF_SIZE  1024u
#define ESP_SERIAL_LINE_BUF_LEN 512u
#define ESP_SERIAL_WORKER_STACK 2048u

struct EspSerial {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* rx_thread;
    EspSerialRxCallback rx_callback;
    void* rx_callback_ctx;
    Expansion* expansion;
    volatile bool connected;
    volatile bool running;
    EspSerialConfig config;
};

/* ISR -- pushes raw bytes into the stream buffer. */
static void
    esp_serial_isr_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    EspSerial* uart = (EspSerial*)context;

    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(uart->rx_stream, &byte, 1, 0);
    }
}

/* Worker thread -- assembles bytes into lines, fires callback. */
static int32_t esp_serial_rx_worker(void* context) {
    EspSerial* uart = (EspSerial*)context;
    const char* tag = uart->config.tag ? uart->config.tag : "EspSerial";

    char line_buf[ESP_SERIAL_LINE_BUF_LEN];
    size_t line_pos = 0;

    FURI_LOG_I(tag, "UART worker started");

    while(uart->running) {
        uint8_t byte;
        size_t received = furi_stream_buffer_receive(
            uart->rx_stream, &byte, 1, furi_ms_to_ticks(100));

        if(received == 0) continue;

        if(byte == '\r') continue;

        if(byte == '\n') {
            line_buf[line_pos] = '\0';

            if(line_pos > 0 && strncmp(line_buf, "[BUF/", 5) != 0) {
                if(!uart->connected) {
                    if(uart->config.connected_check) {
                        if(uart->config.connected_check(line_buf)) {
                            uart->connected = true;
                            FURI_LOG_I(tag, "Board connected (predicate match)");
                        }
                    } else {
                        uart->connected = true;
                        FURI_LOG_I(tag, "Board connected");
                    }
                }

                if(uart->rx_callback) {
                    uart->rx_callback(line_buf, uart->rx_callback_ctx);
                }
            }

            line_pos = 0;
        } else {
            if(line_pos < ESP_SERIAL_LINE_BUF_LEN - 1) {
                line_buf[line_pos++] = (char)byte;
            }
        }
    }

    FURI_LOG_I(tag, "UART worker stopped");
    return 0;
}

EspSerial* esp_serial_alloc(const EspSerialConfig* config) {
    furi_assert(config);

    EspSerial* uart = malloc(sizeof(EspSerial));
    memset(uart, 0, sizeof(EspSerial));
    uart->config = *config;

    const char* tag = config->tag ? config->tag : "EspSerial";

    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    uart->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!uart->serial) {
        FURI_LOG_W(tag, "Failed to acquire USART");
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        uart->expansion = NULL;
        return uart;
    }

    furi_hal_serial_init(uart->serial, 115200);

    uart->rx_stream = furi_stream_buffer_alloc(ESP_SERIAL_RX_BUF_SIZE, 1);

    uart->running = true;
    const char* thread_name = config->thread_name ? config->thread_name : "EspSerialRx";
    uart->rx_thread =
        furi_thread_alloc_ex(thread_name, ESP_SERIAL_WORKER_STACK, esp_serial_rx_worker, uart);
    furi_thread_start(uart->rx_thread);

    furi_hal_serial_async_rx_start(uart->serial, esp_serial_isr_rx_cb, uart, false);

    if(config->probe_on_open) {
        const uint8_t nl = '\n';
        furi_hal_serial_tx(uart->serial, &nl, 1);
        furi_hal_serial_tx_wait_complete(uart->serial);
    }

    FURI_LOG_I(tag, "UART initialised at 115200");
    return uart;
}

void esp_serial_free(EspSerial* uart) {
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

void esp_serial_send(EspSerial* uart, const char* cmd) {
    furi_assert(uart);
    if(!uart->serial || !cmd) return;

    size_t len = strlen(cmd);
    furi_hal_serial_tx(uart->serial, (const uint8_t*)cmd, len);

    const uint8_t nl = '\n';
    furi_hal_serial_tx(uart->serial, &nl, 1);
    furi_hal_serial_tx_wait_complete(uart->serial);

    const char* tag = uart->config.tag ? uart->config.tag : "EspSerial";
    FURI_LOG_D(tag, "TX: %s", cmd);
}

void esp_serial_set_rx_callback(EspSerial* uart, EspSerialRxCallback cb, void* ctx) {
    furi_assert(uart);
    /* Disable first, barrier, set context, barrier, enable.
     * Guarantees the worker never invokes cb with a stale ctx. */
    uart->rx_callback = NULL;
    __DMB();
    uart->rx_callback_ctx = ctx;
    __DMB();
    uart->rx_callback = cb;
}

bool esp_serial_is_connected(EspSerial* uart) {
    furi_assert(uart);
    return uart->connected;
}
