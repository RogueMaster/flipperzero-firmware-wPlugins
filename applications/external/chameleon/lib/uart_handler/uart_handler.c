#include "uart_handler.h"
#include <furi.h>
#include <furi_hal_usb_cdc.h>
#include <string.h>

#define TAG "UartHandler"

struct UartHandler {
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream;
    UartHandlerRxCallback rx_callback;
    void* rx_context;
    bool initialized;
    bool running;
};

static int32_t uart_handler_rx_thread(void* context) {
    UartHandler* handler = context;
    uint8_t buffer[UART_RX_BUFFER_SIZE];

    FURI_LOG_I(TAG, "RX thread started");

    while(handler->running) {
        // Read from USB CDC (interface 0)
        size_t received = furi_hal_cdc_receive(0, buffer, sizeof(buffer));

        if(received > 0) {
            FURI_LOG_D(TAG, "Received %zu bytes", received);

            // Write to stream buffer
            furi_stream_buffer_send(handler->rx_stream, buffer, received, FuriWaitForever);

            // Call callback if set
            if(handler->rx_callback) {
                handler->rx_callback(buffer, received, handler->rx_context);
            }
        }

        furi_delay_ms(10);
    }

    FURI_LOG_I(TAG, "RX thread stopped");
    return 0;
}

UartHandler* uart_handler_alloc() {
    UartHandler* handler = malloc(sizeof(UartHandler));
    memset(handler, 0, sizeof(UartHandler));

    handler->rx_stream = furi_stream_buffer_alloc(UART_RX_BUFFER_SIZE, 1);

    return handler;
}

void uart_handler_free(UartHandler* handler) {
    furi_assert(handler);

    if(handler->initialized) {
        uart_handler_deinit(handler);
    }

    furi_stream_buffer_free(handler->rx_stream);
    free(handler);
}

bool uart_handler_init(UartHandler* handler) {
    furi_assert(handler);

    if(handler->initialized) {
        FURI_LOG_W(TAG, "Already initialized");
        return true;
    }

    FURI_LOG_I(TAG, "Initializing UART/USB CDC");

    // USB CDC is automatically initialized by Flipper OS
    // We just need to ensure it's available

    handler->initialized = true;

    FURI_LOG_I(TAG, "UART/USB CDC initialized");
    return true;
}

void uart_handler_deinit(UartHandler* handler) {
    furi_assert(handler);

    if(!handler->initialized) {
        return;
    }

    uart_handler_stop_rx(handler);

    handler->initialized = false;

    FURI_LOG_I(TAG, "UART/USB CDC deinitialized");
}

void uart_handler_set_rx_callback(
    UartHandler* handler,
    UartHandlerRxCallback callback,
    void* context) {
    furi_assert(handler);
    handler->rx_callback = callback;
    handler->rx_context = context;
}

bool uart_handler_send(UartHandler* handler, const uint8_t* data, size_t length) {
    furi_assert(handler);
    furi_assert(data);

    if(!handler->initialized) {
        FURI_LOG_E(TAG, "Not initialized");
        return false;
    }

    FURI_LOG_D(TAG, "Sending %zu bytes", length);

    // Send via USB CDC (interface 0)
    furi_hal_cdc_send(0, (uint8_t*)data, length);

    return true;
}

bool uart_handler_is_connected(UartHandler* handler) {
    furi_assert(handler);
    return handler->initialized;
}

void uart_handler_start_rx(UartHandler* handler) {
    furi_assert(handler);

    if(handler->running) {
        FURI_LOG_W(TAG, "RX already running");
        return;
    }

    handler->running = true;

    handler->rx_thread = furi_thread_alloc();
    furi_thread_set_name(handler->rx_thread, "UartRxThread");
    furi_thread_set_stack_size(handler->rx_thread, 2048);
    furi_thread_set_context(handler->rx_thread, handler);
    furi_thread_set_callback(handler->rx_thread, uart_handler_rx_thread);
    furi_thread_start(handler->rx_thread);

    FURI_LOG_I(TAG, "RX started");
}

void uart_handler_stop_rx(UartHandler* handler) {
    furi_assert(handler);

    if(!handler->running) {
        return;
    }

    handler->running = false;

    if(handler->rx_thread) {
        furi_thread_join(handler->rx_thread);
        furi_thread_free(handler->rx_thread);
        handler->rx_thread = NULL;
    }

    FURI_LOG_I(TAG, "RX stopped");
}
