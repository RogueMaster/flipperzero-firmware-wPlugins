#include "ld2410_human_detector_uart.h"

#define UART_CH       FuriHalSerialIdUsart
#define UART_BAUDRATE 256000
#define RX_BUF_SIZE   1024

struct LD2410HumanDetectorUart {
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;
    LD2410HumanDetectorUartCallback callback;
    void* callback_context;
    LD2410Parser parser;
    volatile bool worker_running;
    volatile uint32_t total_rx_bytes;
};

// ISR Callback
static void ld2410_human_detector_uart_on_irq_rx(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    LD2410HumanDetectorUart* uart = (LD2410HumanDetectorUart*)context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        uart->total_rx_bytes++;
        furi_stream_buffer_send(uart->rx_stream, &data, 1, 0);
    }
}

uint32_t ld2410_human_detector_uart_get_rx_bytes(LD2410HumanDetectorUart* uart) {
    if(!uart) return 0;
    return uart->total_rx_bytes;
}

// Worker Thread
static int32_t ld2410_human_detector_uart_worker(void* context) {
    LD2410HumanDetectorUart* uart = (LD2410HumanDetectorUart*)context;
    uint8_t data_buf[64];

    while(uart->worker_running) {
        size_t received =
            furi_stream_buffer_receive(uart->rx_stream, data_buf, sizeof(data_buf), 100);

        if(received > 0) {
            for(size_t i = 0; i < received; i++) {
                LD2410Data sensor_data;
                if(ld2410_human_detector_parser_push_byte(
                       &uart->parser, data_buf[i], &sensor_data)) {
                    // Packet complete
                    if(uart->callback) {
                        uart->callback(&sensor_data, uart->callback_context);
                    }
                }
            }
        }
    }

    return 0;
}

LD2410HumanDetectorUart* ld2410_human_detector_uart_alloc() {
    LD2410HumanDetectorUart* uart = malloc(sizeof(LD2410HumanDetectorUart));
    furi_check(uart);
    memset(uart, 0, sizeof(LD2410HumanDetectorUart));

    uart->rx_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    furi_check(uart->rx_stream);

    uart->thread = furi_thread_alloc();
    furi_check(uart->thread);

    furi_thread_set_name(uart->thread, "LD2410HumanDetectorUartWorker");
    furi_thread_set_stack_size(uart->thread, 2048);
    furi_thread_set_callback(uart->thread, ld2410_human_detector_uart_worker);
    furi_thread_set_context(uart->thread, uart);

    ld2410_human_detector_parser_init(&uart->parser);

    return uart;
}

void ld2410_human_detector_uart_free(LD2410HumanDetectorUart* uart) {
    furi_assert(uart);
    ld2410_human_detector_uart_stop(uart);
    furi_thread_free(uart->thread);
    furi_stream_buffer_free(uart->rx_stream);
    free(uart);
}

bool ld2410_human_detector_uart_start(LD2410HumanDetectorUart* uart) {
    furi_assert(uart);
    uart->worker_running = true;

    // Acquire UART and configure with retry
    int attempts = 20; // Increased to 200ms wait
    while(attempts > 0) {
        uart->serial_handle = furi_hal_serial_control_acquire(UART_CH);
        if(uart->serial_handle) break;
        furi_delay_ms(10);
        attempts--;
    }

    if(!uart->serial_handle) {
        FURI_LOG_E("LD2410HumanDetector", "Failed to acquire UART handle");
        uart->worker_running = false;
        return false;
    }

    furi_hal_serial_init(uart->serial_handle, UART_BAUDRATE);

    // Enable RX
    furi_hal_serial_async_rx_start(
        uart->serial_handle, ld2410_human_detector_uart_on_irq_rx, uart, false);

    furi_thread_start(uart->thread);
    return true;
}

void ld2410_human_detector_uart_stop(LD2410HumanDetectorUart* uart) {
    furi_assert(uart);

    if(uart->worker_running) {
        uart->worker_running = false;

        // Stop RX first
        if(uart->serial_handle) {
            furi_hal_serial_async_rx_stop(uart->serial_handle);
        }

        // Wait for thread to finish
        furi_thread_join(uart->thread);

        // Deinit and release
        if(uart->serial_handle) {
            // Skipping deinit to prevent crash
            furi_hal_serial_control_release(uart->serial_handle);
            uart->serial_handle = NULL;
        }

        // Give hardware time to settle
        furi_delay_ms(200);
    }
}

void ld2410_human_detector_uart_set_handle_rx_data_cb(
    LD2410HumanDetectorUart* uart,
    LD2410HumanDetectorUartCallback callback,
    void* context) {
    uart->callback = callback;
    uart->callback_context = context;
}
