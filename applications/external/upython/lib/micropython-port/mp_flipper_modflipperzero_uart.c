#include <furi.h>
#include <furi_hal.h>

#include <mp_flipper_modflipperzero.h>

typedef struct {
    FuriHalSerialHandle* handle;
    FuriStreamBuffer* rx_buffer;
} mp_flipper_uart_ctx_t;

static void on_uart_rx(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    FuriStreamBuffer* buffer = context;

    if(event & FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(buffer, &data, 1, 0);
    }
}

inline void* mp_flipper_uart_open(uint8_t raw_mode, uint32_t baud_rate) {
    FuriHalSerialId mode = raw_mode == MP_FLIPPER_UART_MODE_USART ? FuriHalSerialIdUsart :
                                                                    FuriHalSerialIdLpuart;

    if(furi_hal_serial_control_is_busy(mode)) {
        return NULL;
    }

    mp_flipper_uart_ctx_t* ctx = malloc(sizeof(mp_flipper_uart_ctx_t));

    ctx->handle = furi_hal_serial_control_acquire(mode);
    ctx->rx_buffer = furi_stream_buffer_alloc(512, 1);

    furi_hal_serial_init(ctx->handle, baud_rate);

    furi_hal_serial_async_rx_start(ctx->handle, on_uart_rx, ctx->rx_buffer, false);

    return ctx;
}

inline bool mp_flipper_uart_close(void* handle) {
    mp_flipper_uart_ctx_t* ctx = handle;

    furi_hal_serial_deinit(ctx->handle);
    furi_hal_serial_control_release(ctx->handle);

    furi_stream_buffer_free(ctx->rx_buffer);

    free(ctx);

    return true;
}

inline bool mp_flipper_uart_sync(void* handle) {
    mp_flipper_uart_ctx_t* ctx = handle;

    furi_hal_serial_tx_wait_complete(ctx->handle);
}

inline size_t mp_flipper_uart_read(void* handle, void* buffer, size_t size, int* errcode) {
    mp_flipper_uart_ctx_t* ctx = handle;

    size_t read = 0;
    size_t total = 0;
    size_t left = size;

    do {
        read = furi_stream_buffer_receive(ctx->rx_buffer, &buffer[read], left, 0);
        total += read;
        left -= read;
    } while(read > 0);

    return total;
}

inline size_t mp_flipper_uart_write(void* handle, const void* buffer, size_t size, int* errcode) {
    mp_flipper_uart_ctx_t* ctx = handle;

    furi_hal_serial_tx(ctx->handle, buffer, size);

    return size;
}
