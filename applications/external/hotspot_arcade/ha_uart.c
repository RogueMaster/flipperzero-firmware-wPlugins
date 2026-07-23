#include "ha_uart.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>

// GPIO USART on pins 13/14 — the one the WiFi dev board talks over.
#define UART_CH        (FuriHalSerialIdUsart)
#define RX_STREAM_SIZE (2048)

struct HaUart {
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream; // IRQ producer -> GUI consumer (SPSC)
    FuriHalSerialHandle* serial_handle;
    Expansion* expansion;
    uint32_t baudrate; // normal session baud, restored by ha_uart_resume()
    HaUartNotify notify;
    void* notify_ctx;
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

static void
    ha_uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent ev, void* context) {
    HaUart* uart = context;
    if(ev == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(uart->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
    }
}

// The worker only bridges the ISR to the consumer: it calls notify() (which posts
// a ViewDispatcher custom event; that is thread-safe but NOT ISR-safe, so it can't
// run in the IRQ). It touches no app state, so there are no data races.
static int32_t ha_uart_worker(void* context) {
    HaUart* uart = context;
    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);
        if(events & WorkerEvtStop) break;
        if(events & WorkerEvtRxDone) {
            if(uart->notify) uart->notify(uart->notify_ctx);
        }
    }
    return 0;
}

HaUart* ha_uart_init(uint32_t baudrate, HaUartNotify notify, void* notify_ctx) {
    HaUart* uart = malloc(sizeof(HaUart));
    memset(uart, 0, sizeof(HaUart));
    uart->notify = notify;
    uart->notify_ctx = notify_ctx;
    uart->baudrate = baudrate;

    uart->rx_stream = furi_stream_buffer_alloc(RX_STREAM_SIZE, 1);
    uart->rx_thread = furi_thread_alloc();
    furi_thread_set_name(uart->rx_thread, "HaUartRx");
    furi_thread_set_stack_size(uart->rx_thread, 1024);
    furi_thread_set_context(uart->rx_thread, uart);
    furi_thread_set_callback(uart->rx_thread, ha_uart_worker);
    furi_thread_start(uart->rx_thread);

    // The expansion-module service owns the GPIO USART by default: disable it
    // before acquiring the serial handle (else acquire returns NULL and
    // furi_check aborts), re-enable after release.
    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    uart->serial_handle = furi_hal_serial_control_acquire(UART_CH);
    furi_check(uart->serial_handle);
    furi_hal_serial_init(uart->serial_handle, baudrate);
    furi_hal_serial_async_rx_start(uart->serial_handle, ha_uart_on_irq_cb, uart, false);

    return uart;
}

void ha_uart_stop_rx(HaUart* uart) {
    furi_assert(uart);
    if(!uart->rx_thread) return;
    furi_hal_serial_async_rx_stop(uart->serial_handle);
    furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furi_thread_join(uart->rx_thread);
    furi_thread_free(uart->rx_thread);
    uart->rx_thread = NULL;
}

void ha_uart_deinit(HaUart* uart) {
    furi_assert(uart);
    ha_uart_stop_rx(uart); // idempotent

    furi_hal_serial_deinit(uart->serial_handle);
    furi_hal_serial_control_release(uart->serial_handle);
    furi_stream_buffer_free(uart->rx_stream);

    expansion_enable(uart->expansion);
    furi_record_close(RECORD_EXPANSION);

    free(uart);
}

size_t ha_uart_rx(HaUart* uart, uint8_t* buf, size_t maxlen) {
    return furi_stream_buffer_receive(uart->rx_stream, buf, maxlen, 0);
}

void ha_uart_tx(HaUart* uart, const uint8_t* data, size_t len) {
    if(uart->serial_handle) {
        furi_hal_serial_tx(uart->serial_handle, data, len);
    }
}

FuriHalSerialHandle* ha_uart_serial(HaUart* uart) {
    return uart->serial_handle;
}

void ha_uart_suspend(HaUart* uart) {
    // Stop consuming RX so the flasher owns the line; the worker just parks on its
    // flag-wait (no RxDone events arrive) and needs no teardown.
    furi_hal_serial_async_rx_stop(uart->serial_handle);
}

void ha_uart_resume(HaUart* uart) {
    furi_hal_serial_set_br(uart->serial_handle, uart->baudrate);
    furi_hal_serial_async_rx_start(uart->serial_handle, ha_uart_on_irq_cb, uart, false);
}
