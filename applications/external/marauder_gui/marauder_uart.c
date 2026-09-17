#include "marauder_uart.h"

#include <furi_hal_serial_control.h>
#include <furi_hal_serial.h>
#include <expansion/expansion.h>
#include <string.h>
#include <stdlib.h>

/* Flipper GPIO 13 (TX) / 14 (RX), matching ESP32 Marauder's Serial.begin(115200) on UART0 */
#define MARAUDER_UART_BAUD           115200
#define MARAUDER_UART_RX_STREAM_SIZE 2048

/* Marauder wraps raw PCAP payload in these ASCII markers when a command is run with "-serial".
   Both are exactly 11 bytes; the demux below matches them byte-by-byte and never lets the marker
   bytes themselves reach either stream. Ported from the WiFi Marauder companion app's uart. */
#define MARAUDER_MARK_LEN 11
static const char MARAUDER_MARK_BEGIN[MARAUDER_MARK_LEN] = "[BUF/BEGIN]";
static const char MARAUDER_MARK_CLOSE[MARAUDER_MARK_LEN] = "[BUF/CLOSE]";

struct MarauderUart {
    FuriHalSerialHandle* serial_handle;
    FuriStreamBuffer* rx_stream;
    FuriStreamBuffer* pcap_stream;
    Expansion* expansion;
    bool pcap; /* true while inside a [BUF/BEGIN]..[BUF/CLOSE] region */
    uint8_t mark_test_buf[MARAUDER_MARK_LEN];
    uint8_t mark_test_idx;
};

/* Feed one received byte through the marker state machine, routing it to the pcap stream while
   inside a marked region and to the text stream otherwise. A run of bytes that starts matching a
   marker but then diverges is flushed to whichever stream is currently active, so nothing is
   lost when "[..." turns out to be ordinary text. */
static void marauder_uart_route_byte(MarauderUart* uart, uint8_t data) {
    if(uart->mark_test_idx != 0) {
        /* Mid-match: does this byte continue either marker? */
        if(data == (uint8_t)MARAUDER_MARK_BEGIN[uart->mark_test_idx] ||
           data == (uint8_t)MARAUDER_MARK_CLOSE[uart->mark_test_idx]) {
            uart->mark_test_buf[uart->mark_test_idx++] = data;
            if(uart->mark_test_idx == MARAUDER_MARK_LEN) {
                if(!memcmp(uart->mark_test_buf, MARAUDER_MARK_BEGIN, MARAUDER_MARK_LEN)) {
                    uart->pcap = true;
                } else if(!memcmp(uart->mark_test_buf, MARAUDER_MARK_CLOSE, MARAUDER_MARK_LEN)) {
                    uart->pcap = false;
                }
                uart->mark_test_idx = 0;
            }
            return;
        }
        /* Diverged - the buffered bytes were just data after all, flush them. */
        FuriStreamBuffer* stream = uart->pcap ? uart->pcap_stream : uart->rx_stream;
        furi_stream_buffer_send(stream, uart->mark_test_buf, uart->mark_test_idx, 0);
        uart->mark_test_idx = 0;
    }

    if(data == (uint8_t)MARAUDER_MARK_BEGIN[0]) {
        /* Possible marker start ('['), begin buffering. */
        uart->mark_test_buf[uart->mark_test_idx++] = data;
    } else {
        FuriStreamBuffer* stream = uart->pcap ? uart->pcap_stream : uart->rx_stream;
        furi_stream_buffer_send(stream, &data, 1, 0);
    }
}

static void marauder_uart_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    MarauderUart* uart = context;

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            marauder_uart_route_byte(uart, furi_hal_serial_async_rx(handle));
        }
    }
}

MarauderUart* marauder_uart_alloc(void) {
    MarauderUart* uart = malloc(sizeof(MarauderUart));

    uart->rx_stream = furi_stream_buffer_alloc(MARAUDER_UART_RX_STREAM_SIZE, 1);
    uart->pcap_stream = furi_stream_buffer_alloc(MARAUDER_UART_RX_STREAM_SIZE, 1);
    uart->pcap = false;
    uart->mark_test_idx = 0;

    /* Expansion module detection listens on the same USART pins (13/14) by default and must
       release them before we can acquire the port, or furi_hal_serial_control_acquire()
       returns NULL. */
    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    uart->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_check(uart->serial_handle);

    furi_hal_serial_init(uart->serial_handle, MARAUDER_UART_BAUD);
    furi_hal_serial_async_rx_start(uart->serial_handle, marauder_uart_rx_callback, uart, false);

    return uart;
}

void marauder_uart_free(MarauderUart* uart) {
    furi_hal_serial_async_rx_stop(uart->serial_handle);
    furi_hal_serial_deinit(uart->serial_handle);
    furi_hal_serial_control_release(uart->serial_handle);

    expansion_enable(uart->expansion);
    furi_record_close(RECORD_EXPANSION);

    furi_stream_buffer_free(uart->rx_stream);
    furi_stream_buffer_free(uart->pcap_stream);

    free(uart);
}

void marauder_uart_send(MarauderUart* uart, const char* data) {
    furi_hal_serial_tx(uart->serial_handle, (const uint8_t*)data, strlen(data));
}

void marauder_uart_send_line(MarauderUart* uart, const char* line) {
    marauder_uart_send(uart, line);
    marauder_uart_send(uart, "\n");
}

size_t marauder_uart_receive(MarauderUart* uart, uint8_t* buffer, size_t max_len) {
    return furi_stream_buffer_receive(uart->rx_stream, buffer, max_len, 0);
}

size_t marauder_uart_receive_pcap(MarauderUart* uart, uint8_t* buffer, size_t max_len) {
    return furi_stream_buffer_receive(uart->pcap_stream, buffer, max_len, 0);
}

void marauder_uart_reset_capture(MarauderUart* uart) {
    uart->pcap = false;
    uart->mark_test_idx = 0;
    furi_stream_buffer_reset(uart->rx_stream);
    furi_stream_buffer_reset(uart->pcap_stream);
}
