#include "gravity_app_i.h"
#include "gravity_uart.h"
#include <expansion/expansion.h>

static FuriHalSerialHandle* gravity_serial_handle;
static Expansion* gravity_expansion;

//#define UART_CH (FuriHalUartIdUSART1)
//#define BAUDRATE (115200)

struct GravityUart {
    GravityApp* app;
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream;
    uint8_t rx_buf[RX_BUF_SIZE + 1];
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context);
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

void gravity_uart_set_handle_rx_data_cb(
    GravityUart* uart,
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context)) {
    furi_assert(uart);
    uart->handle_rx_data_cb = handle_rx_data_cb;
}

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

void gravity_uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent ev, void* context) {
    GravityUart* uart = (GravityUart*)context;

    if(ev == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(uart->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
    }
}

static int32_t uart_worker(void* context) {
    GravityUart* uart = (void*)context;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);
        if(events & WorkerEvtStop) break;
        if(events & WorkerEvtRxDone) {
            size_t len = furi_stream_buffer_receive(uart->rx_stream, uart->rx_buf, RX_BUF_SIZE, 0);
            if(len > 0) {
                if(uart->handle_rx_data_cb) uart->handle_rx_data_cb(uart->rx_buf, len, uart->app);
            }
        }
    }

    furi_stream_buffer_free(uart->rx_stream);

    return 0;
}

void gravity_uart_tx(uint8_t* data, size_t len) {
    furi_hal_serial_tx(gravity_serial_handle, data, len);
}

GravityUart* gravity_uart_init(GravityApp* app) {
    GravityUart* uart = malloc(sizeof(GravityUart));
    uart->app = app;
    // Init all rx stream and thread early to avoid crashes
    uart->rx_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    uart->rx_thread = furi_thread_alloc();
    furi_thread_set_name(uart->rx_thread, "GravityUartRxThread");
    furi_thread_set_stack_size(uart->rx_thread, 1024);
    furi_thread_set_context(uart->rx_thread, uart);
    furi_thread_set_callback(uart->rx_thread, uart_worker);

    furi_thread_start(uart->rx_thread);

    gravity_expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(gravity_expansion);
    gravity_serial_handle = furi_hal_serial_control_acquire(UART_CH);
    furi_check(gravity_serial_handle);
    if(app->BAUDRATE == 0) {
        app->BAUDRATE = 115200;
    }
    furi_hal_serial_init(gravity_serial_handle, app->BAUDRATE);
    furi_hal_serial_async_rx_start(gravity_serial_handle, gravity_uart_on_irq_cb, uart, false);

    return uart;
}

void gravity_uart_free(GravityUart* uart) {
    furi_assert(uart);

    furi_hal_serial_async_rx_stop(gravity_serial_handle);
    furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furi_thread_join(uart->rx_thread);
    furi_thread_free(uart->rx_thread);

    furi_hal_serial_deinit(gravity_serial_handle);
    furi_hal_serial_control_release(gravity_serial_handle);
    gravity_serial_handle = NULL;
    expansion_enable(gravity_expansion);
    furi_record_close(RECORD_EXPANSION);
    gravity_expansion = NULL;

    free(uart);
}
