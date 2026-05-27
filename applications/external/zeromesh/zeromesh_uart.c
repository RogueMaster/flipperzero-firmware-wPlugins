#include "zeromesh_uart.h"
#include "zeromesh_history.h"

#define TAG "zeromesh_serial"

static void rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    ZeroMeshApp* app = (ZeroMeshApp*)ctx;
    if(!app) return;

    if(event == FuriHalSerialRxEventData) {
        uint8_t b = furi_hal_serial_async_rx(handle);
        app->rx_bytes++;
        furi_stream_buffer_send(app->rx_stream, &b, 1, 0);

        if(app->rx_bytes < 20) {
            FURI_LOG_I(TAG, "RX byte %lu: 0x%02X", (unsigned long)app->rx_bytes, b);
        }
    }
}

void uart_close(ZeroMeshApp* app) {
    if(!app) return;

    if(app->serial) {
        furi_hal_serial_async_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        app->serial = NULL;
    }
}

void uart_open(ZeroMeshApp* app) {
    if(!app) return;

    uart_close(app);

    app->serial = furi_hal_serial_control_acquire(app->uart_id);
    furi_hal_serial_init(app->serial, app->baud);
    furi_hal_serial_async_rx_start(app->serial, rx_cb, app, false);

    log_line(app, "UART: %s @ %lu",
             (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART",
             (unsigned long)app->baud);

    char status_msg[64];
    snprintf(status_msg, sizeof(status_msg), "%s @ %lu",
             (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART",
             (unsigned long)app->baud);
    set_status(app, status_msg);
}

void uart_reopen(ZeroMeshApp* app, FuriHalSerialId new_id, uint32_t new_baud) {
    if(!app) return;
    app->uart_id = new_id;
    app->baud = new_baud;
    uart_open(app);
}