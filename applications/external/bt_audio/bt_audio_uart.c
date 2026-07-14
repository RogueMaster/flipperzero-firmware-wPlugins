#include "bt_audio.h"
#include <furi_hal_serial.h>
#include <string.h>

struct BtAudioUart {
    BtAudio* app;
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;
    void (*rx_callback)(const char* data, size_t len, void* context);
    uint8_t rx_buf[BT_AUDIO_RX_BUF_SIZE];
    char line_buf[BT_AUDIO_RX_BUF_SIZE];
    size_t line_pos;
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

static void bt_audio_uart_on_irq_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    BtAudioUart* uart = (BtAudioUart*)context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(uart->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
    }
}

static int32_t uart_worker(void* context) {
    BtAudioUart* uart = (BtAudioUart*)context;

    while(1) {
        uint32_t events = furi_thread_flags_wait(
            WorkerEvtStop | WorkerEvtRxDone, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEvtStop) break;

        if(events & WorkerEvtRxDone) {
            size_t len =
                furi_stream_buffer_receive(uart->rx_stream, uart->rx_buf, BT_AUDIO_RX_BUF_SIZE, 0);
            if(len > 0) {
                // Process received data line by line
                for(size_t i = 0; i < len; i++) {
                    char c = uart->rx_buf[i];
                    if(c == '\n' || c == '\r') {
                        if(uart->line_pos > 0) {
                            uart->line_buf[uart->line_pos] = '\0';
                            if(uart->rx_callback) {
                                uart->rx_callback(uart->line_buf, uart->line_pos, uart->app);
                            }
                            uart->line_pos = 0;
                        }
                    } else if(uart->line_pos < sizeof(uart->line_buf) - 1) {
                        uart->line_buf[uart->line_pos++] = c;
                    }
                }
            }
        }
    }

    furi_stream_buffer_free(uart->rx_stream);
    return 0;
}

BtAudioUart* bt_audio_uart_init(BtAudio* app) {
    BtAudioUart* uart = malloc(sizeof(BtAudioUart));
    uart->app = app;
    uart->rx_callback = NULL;
    uart->line_pos = 0;
    uart->serial_handle = NULL;

    // Initialize stream buffer and thread
    uart->rx_stream = furi_stream_buffer_alloc(BT_AUDIO_RX_BUF_SIZE, 1);
    uart->rx_thread = furi_thread_alloc();
    furi_thread_set_name(uart->rx_thread, "BtAudioUartRx");
    furi_thread_set_stack_size(uart->rx_thread, 2048); // Increased from 1024 for better stability
    furi_thread_set_context(uart->rx_thread, uart);
    furi_thread_set_callback(uart->rx_thread, uart_worker);
    furi_thread_start(uart->rx_thread);

    // Small delay to let power stabilize after OTG enable
    furi_delay_ms(50);

    // Initialize UART - handle gracefully if acquisition fails
    uart->serial_handle = furi_hal_serial_control_acquire(BT_AUDIO_UART_CH);
    if(uart->serial_handle) {
        furi_hal_serial_init(uart->serial_handle, BT_AUDIO_BAUDRATE);
        furi_hal_serial_async_rx_start(uart->serial_handle, bt_audio_uart_on_irq_cb, uart, false);
        FURI_LOG_I("BtAudioUart", "UART initialized successfully");
    } else {
        FURI_LOG_E("BtAudioUart", "Failed to acquire serial handle - UART disabled");
    }

    return uart;
}

void bt_audio_uart_free(BtAudioUart* uart) {
    furi_assert(uart);

    // Only cleanup serial if it was acquired
    if(uart->serial_handle) {
        furi_hal_serial_async_rx_stop(uart->serial_handle);
        furi_hal_serial_deinit(uart->serial_handle);
        furi_hal_serial_control_release(uart->serial_handle);
    }

    furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furi_thread_join(uart->rx_thread);
    furi_thread_free(uart->rx_thread);

    free(uart);
}

void bt_audio_uart_tx(BtAudioUart* uart, const char* data) {
    // Only transmit if serial handle is valid
    if(uart && uart->serial_handle) {
        furi_hal_serial_tx(uart->serial_handle, (const uint8_t*)data, strlen(data));
    }
}

void bt_audio_uart_set_rx_callback(
    BtAudioUart* uart,
    void (*callback)(const char* data, size_t len, void* context)) {
    furi_assert(uart);
    uart->rx_callback = callback;
}
