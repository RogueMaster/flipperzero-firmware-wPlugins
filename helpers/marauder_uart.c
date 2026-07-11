#include "marauder_uart.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>
#include <string.h>
#include <stdlib.h>

#define UART_BAUD           115200
#define UART_RX_STREAM_SIZE 2048
#define UART_LINE_MAX       160
#define UART_WORKER_STACK   2048

struct MarauderUart {
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial;
    Expansion* expansion;
    MarauderUartChannelId channel;
    volatile bool running;

    MarauderUartLineCallback line_cb;
    void* cb_context;
};

MarauderUart* marauder_uart_alloc(void) {
    MarauderUart* uart = malloc(sizeof(MarauderUart));
    memset(uart, 0, sizeof(MarauderUart));
    uart->channel = MarauderUartChannelUsart;
    return uart;
}

void marauder_uart_free(MarauderUart* uart) {
    furi_assert(uart);
    marauder_uart_stop(uart);
    free(uart);
}

void marauder_uart_set_line_callback(
    MarauderUart* uart,
    MarauderUartLineCallback callback,
    void* context) {
    furi_assert(uart);
    uart->line_cb = callback;
    uart->cb_context = context;
}

void marauder_uart_set_channel(MarauderUart* uart, MarauderUartChannelId channel) {
    furi_assert(uart);
    if(uart->running) return; // channel is latched while the link is up
    uart->channel = channel;
}

/* ---------- worker / ISR ---------- */

static int32_t marauder_uart_worker(void* context) {
    MarauderUart* uart = context;
    char line[UART_LINE_MAX];
    size_t pos = 0;
    uint8_t buf[64];

    while(uart->running) {
        size_t got = furi_stream_buffer_receive(uart->rx_stream, buf, sizeof(buf), 50);
        for(size_t i = 0; i < got; i++) {
            char c = (char)buf[i];
            if(c == '\n' || c == '\r') {
                if(pos > 0) {
                    line[pos] = '\0';
                    if(uart->line_cb) uart->line_cb(uart->cb_context, line);
                    pos = 0;
                }
            } else if(pos < sizeof(line) - 1) {
                line[pos++] = c;
            } else {
                // overrun: flush what we have so a monster line still shows
                line[pos] = '\0';
                if(uart->line_cb) uart->line_cb(uart->cb_context, line);
                pos = 0;
            }
        }
    }
    return 0;
}

static void
    marauder_uart_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    MarauderUart* uart = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(uart->rx_stream, &data, 1, 0);
    }
}

/* ---------- control ---------- */

void marauder_uart_start(MarauderUart* uart) {
    furi_assert(uart);
    if(uart->running) return;

    // The Expansion service squats on the USART by default — take it over so we
    // own the pins for the duration of the link.
    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    uart->rx_stream = furi_stream_buffer_alloc(UART_RX_STREAM_SIZE, 1);
    uart->running = true;

    uart->thread =
        furi_thread_alloc_ex("TridentUart", UART_WORKER_STACK, marauder_uart_worker, uart);
    furi_thread_start(uart->thread);

    FuriHalSerialId id = (uart->channel == MarauderUartChannelLpuart) ? FuriHalSerialIdLpuart :
                                                                        FuriHalSerialIdUsart;
    uart->serial = furi_hal_serial_control_acquire(id);
    furi_check(uart->serial);
    furi_hal_serial_init(uart->serial, UART_BAUD);
    furi_hal_serial_async_rx_start(uart->serial, marauder_uart_rx_isr, uart, false);
}

void marauder_uart_stop(MarauderUart* uart) {
    furi_assert(uart);
    if(!uart->running) return;

    uart->running = false;

    if(uart->serial) {
        furi_hal_serial_async_rx_stop(uart->serial);
        furi_hal_serial_deinit(uart->serial);
        furi_hal_serial_control_release(uart->serial);
        uart->serial = NULL;
    }

    if(uart->thread) {
        // running=false above; the worker's stream receive times out within 50ms and exits
        furi_thread_join(uart->thread);
        furi_thread_free(uart->thread);
        uart->thread = NULL;
    }

    if(uart->rx_stream) {
        furi_stream_buffer_free(uart->rx_stream);
        uart->rx_stream = NULL;
    }

    if(uart->expansion) {
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        uart->expansion = NULL;
    }
}

bool marauder_uart_is_running(MarauderUart* uart) {
    furi_assert(uart);
    return uart->running;
}

void marauder_uart_send(MarauderUart* uart, const char* data) {
    furi_assert(uart);
    if(!uart->running || !uart->serial || !data) return;
    furi_hal_serial_tx(uart->serial, (const uint8_t*)data, strlen(data));
}
