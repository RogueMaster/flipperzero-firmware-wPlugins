#include "wendigo_app_i.h"
#include "wendigo_uart.h"

struct Wendigo_Uart {
    WendigoApp* app;
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream;
    uint8_t rx_buf[RX_BUF_SIZE + 1];
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context);
    FuriHalSerialHandle* serial_handle;
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

void wendigo_uart_set_handle_rx_data_cb(
    Wendigo_Uart* uart,
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context)) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_set_handle_rx_data_cb()");
    furi_assert(uart);
    uart->handle_rx_data_cb = handle_rx_data_cb;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_set_handle_rx_data_cb()");
}

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

void wendigo_uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_on_irq_cb()");
    Wendigo_Uart* uart = (Wendigo_Uart*)context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(uart->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_on_irq_cb()");
}

static int32_t wendigo_worker(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_worker()");
    Wendigo_Uart* uart = (void*)context;

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
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_worker()");
    return 0;
}

void wendigo_uart_tx(Wendigo_Uart* uart, uint8_t* data, size_t len) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_tx()");
    furi_hal_serial_tx(uart->serial_handle, data, len);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_tx()");
}

Wendigo_Uart* wendigo_uart_init(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_init()");
    Wendigo_Uart* uart = malloc(sizeof(Wendigo_Uart));
    uart->app = app;
    // Init all rx stream and thread early to avoid crashes
    uart->rx_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    uart->rx_thread = furi_thread_alloc();
    furi_thread_set_name(uart->rx_thread, "Wendigo_UartRxThread");
    furi_thread_set_stack_size(uart->rx_thread, 1024);
    furi_thread_set_context(uart->rx_thread, uart);
    furi_thread_set_callback(uart->rx_thread, wendigo_worker);

    furi_thread_start(uart->rx_thread);

    if(app->BAUDRATE == 0) {
        app->BAUDRATE = 115200;
    }

    uart->serial_handle = furi_hal_serial_control_acquire(app->uart_ch);
    furi_check(uart->serial_handle);
    furi_hal_serial_init(uart->serial_handle, app->BAUDRATE);

    furi_hal_serial_async_rx_start(uart->serial_handle, wendigo_uart_on_irq_cb, uart, false);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_init()");
    return uart;
}

void wendigo_uart_free(Wendigo_Uart* uart) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_free()");
    furi_assert(uart);

    furi_hal_serial_async_rx_stop(uart->serial_handle);
    furi_hal_serial_deinit(uart->serial_handle);
    furi_hal_serial_control_release(uart->serial_handle);

    furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furi_thread_join(uart->rx_thread);
    furi_thread_free(uart->rx_thread);

    free(uart);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_free()");
}
