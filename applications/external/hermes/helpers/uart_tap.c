#include "uart_tap.h"

#include <furi_hal_serial.h>
#include <string.h>

struct UartTap {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx;
    bool open;
    bool tx_enabled;
    volatile uint32_t dropped;
};

const char* uart_tap_enter_name(UartTapEnter mode) {
    switch(mode) {
    case UartTapEnterLf:
        return "LF";
    case UartTapEnterCrLf:
        return "CRLF";
    case UartTapEnterCr:
    default:
        return "CR";
    }
}

/* DMA rather than per-byte interrupts: the console has to keep up with a full
 * boot log at 921600 without losing the middle of it. */
static void uart_tap_rx_isr(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    size_t data_len,
    void* ctx) {
    UartTap* tap = ctx;

    if(!(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle))) return;
    if(data_len == 0) return;

    uint8_t chunk[FURI_HAL_SERIAL_DMA_BUFFER_SIZE];
    while(data_len > 0) {
        const size_t want = (data_len > sizeof(chunk)) ? sizeof(chunk) : data_len;
        const size_t got = furi_hal_serial_dma_rx(handle, chunk, want);
        if(got == 0) break;
        data_len -= got;

        const size_t sent = furi_stream_buffer_send(tap->rx, chunk, got, 0);
        if(sent < got) tap->dropped += (uint32_t)(got - sent);
    }
}

UartTap* uart_tap_alloc(void) {
    UartTap* tap = malloc(sizeof(UartTap));
    memset(tap, 0, sizeof(UartTap));
    tap->rx = furi_stream_buffer_alloc(UART_TAP_BUFFER, 1);
    return tap;
}

void uart_tap_free(UartTap* tap) {
    furi_assert(tap);
    uart_tap_close(tap);
    furi_stream_buffer_free(tap->rx);
    free(tap);
}

bool uart_tap_open(
    UartTap* tap,
    HermesPort port,
    uint32_t baud,
    HermesFraming framing,
    bool tx_enabled) {
    furi_assert(tap);
    if(tap->open) uart_tap_close(tap);

    tap->serial = furi_hal_serial_control_acquire(hermes_port_serial_id(port));
    if(!tap->serial) return false;

    furi_stream_buffer_reset(tap->rx);
    tap->dropped = 0;

    furi_hal_serial_init(tap->serial, baud);
    hermes_framing_apply(tap->serial, framing);

    /* init() grabs both pins; hand TX back unless the user actually wants to
     * drive the target's RX line. */
    tap->tx_enabled = tx_enabled;
    if(!tx_enabled) {
        furi_hal_serial_disable_direction(tap->serial, FuriHalSerialDirectionTx);
    }

    furi_hal_serial_dma_rx_start(tap->serial, uart_tap_rx_isr, tap, true);
    tap->open = true;
    return true;
}

void uart_tap_close(UartTap* tap) {
    furi_assert(tap);
    if(!tap->open) return;

    furi_hal_serial_dma_rx_stop(tap->serial);
    furi_hal_serial_deinit(tap->serial);
    furi_hal_serial_control_release(tap->serial);
    tap->serial = NULL;
    tap->open = false;
}

bool uart_tap_is_open(const UartTap* tap) {
    furi_assert(tap);
    return tap->open;
}

size_t uart_tap_read(UartTap* tap, uint8_t* out, size_t max) {
    furi_assert(tap);
    furi_assert(out);
    return furi_stream_buffer_receive(tap->rx, out, max, 0);
}

void uart_tap_send(UartTap* tap, const uint8_t* data, size_t len) {
    furi_assert(tap);
    if(!tap->open || !tap->tx_enabled || len == 0) return;
    furi_hal_serial_tx(tap->serial, data, len);
}

void uart_tap_send_byte(UartTap* tap, uint8_t byte) {
    uart_tap_send(tap, &byte, 1);
}

void uart_tap_send_enter(UartTap* tap, UartTapEnter mode) {
    switch(mode) {
    case UartTapEnterLf:
        uart_tap_send(tap, (const uint8_t*)"\n", 1);
        break;
    case UartTapEnterCrLf:
        uart_tap_send(tap, (const uint8_t*)"\r\n", 2);
        break;
    case UartTapEnterCr:
    default:
        uart_tap_send(tap, (const uint8_t*)"\r", 1);
        break;
    }
}

bool uart_tap_tx_enabled(const UartTap* tap) {
    furi_assert(tap);
    return tap->tx_enabled;
}

uint32_t uart_tap_dropped(const UartTap* tap) {
    furi_assert(tap);
    return tap->dropped;
}
