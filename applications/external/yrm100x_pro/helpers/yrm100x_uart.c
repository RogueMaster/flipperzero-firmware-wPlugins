#include "yrm100x_uart.h"
#include "debug_log.h"

#undef FURI_LOG_D
#undef FURI_LOG_I
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)

/**
 * File that handles the uart for the YRM100
 * @author frux-c
*/
void uhf_uart_default_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* ctx) {
    UHFUart* uart = (UHFUart*)ctx;
    if(!handle || !uart || !uart->buffer) return;

    if((event & FuriHalSerialRxEventData) == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        Buffer* buffer = uart->buffer;

        /*
         * A frame is complete only at the length declared by PL, not at the
         * first 0x7E byte. EPC/User data and even the checksum may legitimately
         * equal 0x7E; closing there truncated a valid reply and made the next
         * command consume its tail.
         */
        /* Keep a completed frame frozen until its consumer explicitly resets it. */
        if(uhf_is_buffer_closed(buffer)) return;

        if(uhf_buffer_get_size(buffer) == 0U) {
            /* Ignore line noise/tails until the next real frame header. */
            if(data != UHF_UART_FRAME_START) return;
            uhf_buffer_reset(buffer);
        }

        if(!uhf_buffer_append_single(buffer, data)) return;

        size_t size = uhf_buffer_get_size(buffer);
        if(size >= 5U) {
            uint8_t* bytes = uhf_buffer_get_data(buffer);
            size_t payload_len = ((size_t)bytes[3] << 8) | bytes[4];
            size_t expected_size = payload_len + 7U;

            if(expected_size > buffer->capacity || expected_size < 7U) {
                uhf_buffer_reset(buffer);
            } else if(size == expected_size) {
                if(data == UHF_UART_FRAME_END) {
                    uhf_buffer_close(buffer);
                } else {
                    bool restart = data == UHF_UART_FRAME_START;
                    uhf_buffer_reset(buffer);
                    if(restart) uhf_buffer_append_single(buffer, data);
                }
            } else if(size > expected_size) {
                bool restart = data == UHF_UART_FRAME_START;
                uhf_buffer_reset(buffer);
                if(restart) uhf_buffer_append_single(buffer, data);
            }
        }
        uhf_uart_tick_reset(uart);
    }
}

UHFUart* uhf_uart_alloc() {
    UHFUart* uart = (UHFUart*)calloc(1, sizeof(UHFUart));
    if(!uart) {
        UHF_E("UART", "ALLOC failed: context");
        uhf_debug_flush();
        return NULL;
    }

    uart->bus = FuriHalBusUSART1;
    uart->handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!uart->handle) {
        UHF_W("UART", "ALLOC failed: USART control busy");
        uhf_debug_flush();
        free(uart);
        return NULL;
    }

    uart->init_by_app = !furi_hal_bus_is_enabled(uart->bus);
    uart->tick = UHF_UART_WAIT_TICK;
    uart->baudrate = UHF_UART_DEFAULT_BAUDRATE;

    if(uart->init_by_app) {
        FURI_LOG_I("UHF_UART", "UHF UART INIT BY APP");
        furi_hal_serial_init(uart->handle, uart->baudrate);
    } else {
        FURI_LOG_I("UHF_UART", "UHF UART INIT BY HAL");
    }

    uart->buffer = uhf_buffer_alloc(UHF_UART_RX_BUFFER_SIZE);
    if(!uart->buffer) {
        UHF_E("UART", "ALLOC failed: RX buffer");
        uhf_debug_flush();
        if(uart->init_by_app) furi_hal_serial_deinit(uart->handle);
        furi_hal_serial_control_release(uart->handle);
        free(uart);
        return NULL;
    }

    furi_hal_serial_async_rx_start(uart->handle, uhf_uart_default_rx_callback, uart, false);

    UHF_I(
        "UART",
        "ALLOC ready uart=%p handle=%p init_by_app=%d baud=%lu",
        (void*)uart,
        (void*)uart->handle,
        uart->init_by_app ? 1 : 0,
        (unsigned long)uart->baudrate);
    uhf_debug_flush();

    return uart;
}

void uhf_uart_free(UHFUart* uart) {
    if(!uart) return;

    /*
     * RX callback context points to UHFUart. Stop interrupt-driven RX before
     * freeing the buffer/context or releasing the serial handle.
     */
    if(uart->handle) {
        furi_hal_serial_async_rx_stop(uart->handle);
        furi_hal_serial_tx_wait_complete(uart->handle);
    }

    if(uart->buffer) {
        uhf_buffer_free(uart->buffer);
        uart->buffer = NULL;
    }

    if(uart->handle) {
        if(uart->init_by_app) {
            furi_hal_serial_deinit(uart->handle);
        }
        furi_hal_serial_control_release(uart->handle);
        uart->handle = NULL;
    }

    free(uart);
}

void uhf_uart_set_receive_byte_callback(
    UHFUart* uart,
    FuriHalSerialAsyncRxCallback callback,
    void* ctx,
    bool report_errors) {
    if(!uart || !uart->handle || !callback) return;
    furi_hal_serial_async_rx_start(uart->handle, callback, ctx, report_errors);
}

void uhf_uart_send(UHFUart* uart, uint8_t* data, size_t size) {
    if(!uart || !uart->handle || !data || size == 0U) return;
    furi_hal_serial_tx(uart->handle, data, size);
}

void uhf_uart_send_wait(UHFUart* uart, uint8_t* data, size_t size) {
    if(!uart || !uart->handle || !data || size == 0U) return;
    uhf_uart_send(uart, data, size);
    furi_hal_serial_tx_wait_complete(uart->handle);
    // furi_thread_flags_set(furi_thread_get_id(uart->thread), UHFUartWorkerWaitingDataFlag);
}

void uhf_uart_set_baudrate(UHFUart* uart, uint32_t baudrate) {
    if(!uart || !uart->handle) return;
    furi_hal_serial_set_br(uart->handle, baudrate);
    uart->baudrate = baudrate;
}

bool uhf_uart_tick(UHFUart* uart) {
    if(!uart) return true;
    if(uart->tick > 0) {
        uart->tick--;
    }
    return uart->tick == 0;
}

void uhf_uart_tick_reset(UHFUart* uart) {
    if(!uart) return;
    uart->tick = UHF_UART_WAIT_TICK;
}
