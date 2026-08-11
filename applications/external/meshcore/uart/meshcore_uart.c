#include "meshcore_uart.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>

#define MESHCORE_UART_RX_ERROR_EVENTS                                        \
    (FuriHalSerialRxEventFrameError | FuriHalSerialRxEventNoiseError |       \
     FuriHalSerialRxEventOverrunError | FuriHalSerialRxEventParityError)

struct MeshCoreUart {
    FuriHalSerialHandle* handle;
    FuriStreamBuffer* rx_stream;
    volatile uint32_t rx_errors; /* written from ISR, read from a worker thread */
};

/* The expansion service is global, not per-port, so with two ports open it
 * must be disabled once and restored only when the last one closes. Opens and
 * closes happen from scene code on one thread, so a plain counter is enough. */
static Expansion* g_expansion;
static uint32_t g_expansion_holders;

static void meshcore_uart_expansion_hold(void) {
    if(g_expansion_holders++ == 0) {
        g_expansion = furi_record_open(RECORD_EXPANSION);
        expansion_disable(g_expansion);
    }
}

static void meshcore_uart_expansion_release(void) {
    furi_assert(g_expansion_holders > 0);
    if(--g_expansion_holders == 0) {
        expansion_enable(g_expansion);
        furi_record_close(RECORD_EXPANSION);
        g_expansion = NULL;
    }
}

/* Interrupt context: drain the peripheral into the stream buffer and get out.
 * A full buffer means the consumer is not keeping up; dropping is preferable
 * to blocking in an ISR, and mc_rx_poll() resyncs past the resulting gap. */
static void meshcore_uart_rx_isr(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    MeshCoreUart* uart = context;

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t byte = furi_hal_serial_async_rx(handle);
            furi_stream_buffer_send(uart->rx_stream, &byte, 1, 0);
        }
    }

    if(event & MESHCORE_UART_RX_ERROR_EVENTS) {
        uart->rx_errors++;
    }
}

MeshCoreUart* meshcore_uart_open(FuriHalSerialId serial_id) {
    MeshCoreUart* uart = malloc(sizeof(MeshCoreUart));
    uart->handle = NULL;
    uart->rx_errors = 0;
    uart->rx_stream = furi_stream_buffer_alloc(MESHCORE_UART_RX_BUFSZ, 1);

    /* The expansion service may be listening on this port; it has to let go
     * first or the acquire below returns NULL. */
    meshcore_uart_expansion_hold();

    uart->handle = furi_hal_serial_control_acquire(serial_id);
    if(!uart->handle) {
        meshcore_uart_expansion_release();
        furi_stream_buffer_free(uart->rx_stream);
        free(uart);
        return NULL;
    }

    furi_hal_serial_init(uart->handle, MESHCORE_UART_BAUD);
    furi_hal_serial_configure_framing(
        uart->handle, FuriHalSerialDataBits8, FuriHalSerialParityNone, FuriHalSerialStopBits1);
    furi_hal_serial_async_rx_start(uart->handle, meshcore_uart_rx_isr, uart, true);

    return uart;
}

void meshcore_uart_close(MeshCoreUart* uart) {
    furi_assert(uart);

    furi_hal_serial_async_rx_stop(uart->handle);
    furi_hal_serial_deinit(uart->handle);
    furi_hal_serial_control_release(uart->handle);

    /* Order matters: enable only after the handle is released (expansion.h). */
    meshcore_uart_expansion_release();

    furi_stream_buffer_free(uart->rx_stream);
    free(uart);
}

void meshcore_uart_tx(MeshCoreUart* uart, const uint8_t* data, size_t len) {
    furi_assert(uart);
    furi_hal_serial_tx(uart->handle, data, len);
}

size_t meshcore_uart_rx(MeshCoreUart* uart, uint8_t* buf, size_t cap, uint32_t timeout_ms) {
    furi_assert(uart);
    return furi_stream_buffer_receive(uart->rx_stream, buf, cap, furi_ms_to_ticks(timeout_ms));
}

void meshcore_uart_rx_flush(MeshCoreUart* uart) {
    furi_assert(uart);
    furi_stream_buffer_reset(uart->rx_stream);
}

uint32_t meshcore_uart_rx_errors(const MeshCoreUart* uart) {
    furi_assert(uart);
    return uart->rx_errors;
}
