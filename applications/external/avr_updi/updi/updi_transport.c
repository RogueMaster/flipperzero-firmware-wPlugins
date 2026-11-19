#include "updi_transport.h"

#include "updi_constants.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_gpio.h>

#include <stdio.h>
#include <string.h>

#define UPDI_RX_STREAM_SIZE 1024u

struct UpdiTransport {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx;
    uint32_t baud;
    UpdiPins pins;
    bool open;
    char error[96];
};

static void updi_transport_set_error(UpdiTransport* transport, const char* message) {
    snprintf(transport->error, sizeof(transport->error), "%s", message);
}

static void
    updi_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UpdiTransport* transport = context;
    if((event & FuriHalSerialRxEventData) == 0) return;
    while(furi_hal_serial_async_rx_available(handle)) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(transport->rx, &byte, 1, 0);
    }
}

static void updi_transport_apply_framing(UpdiTransport* transport) {
    furi_hal_serial_configure_framing(
        transport->serial, FuriHalSerialDataBits8, FuriHalSerialParityEven, FuriHalSerialStopBits2);
}

UpdiTransport* updi_transport_alloc(void) {
    UpdiTransport* transport = malloc(sizeof(UpdiTransport));
    if(transport == NULL) return NULL;
    memset(transport, 0, sizeof(UpdiTransport));
    transport->rx = furi_stream_buffer_alloc(UPDI_RX_STREAM_SIZE, 1);
    if(transport->rx == NULL) {
        free(transport);
        return NULL;
    }
    transport->pins = UpdiPins15_16;
    return transport;
}

void updi_transport_set_pins(UpdiTransport* transport, UpdiPins pins) {
    if(transport == NULL || transport->open) return;
    transport->pins = pins == UpdiPins13_14 ? UpdiPins13_14 : UpdiPins15_16;
}

void updi_transport_free(UpdiTransport* transport) {
    if(transport == NULL) return;
    updi_transport_close(transport);
    furi_stream_buffer_free(transport->rx);
    free(transport);
}

bool updi_transport_open(UpdiTransport* transport) {
    if(transport->open) return true;

    /* 15/16 is LPUART. 13/14 is the external USART. */
    FuriHalSerialId serial_id = transport->pins == UpdiPins13_14 ? FuriHalSerialIdUsart :
                                                                   FuriHalSerialIdLpuart;
    transport->serial = furi_hal_serial_control_acquire(serial_id);
    if(transport->serial == NULL) {
        updi_transport_set_error(
            transport,
            transport->pins == UpdiPins13_14 ? "Pins 13/14 busy. Close USB-UART." :
                                               "Pins 15/16 busy. Close USB-UART.");
        return false;
    }

    transport->baud = UPDI_BAUD_SAFE;
    furi_hal_serial_init(transport->serial, transport->baud);
    updi_transport_apply_framing(transport);
    furi_stream_buffer_reset(transport->rx);
    furi_hal_serial_async_rx_start(transport->serial, updi_rx_callback, transport, false);
    transport->open = true;
    transport->error[0] = '\0';
    return true;
}

void updi_transport_close(UpdiTransport* transport) {
    if(transport == NULL || !transport->open) return;
    furi_hal_serial_async_rx_stop(transport->serial);
    furi_hal_serial_tx_wait_complete(transport->serial);
    furi_hal_serial_deinit(transport->serial);
    furi_hal_serial_control_release(transport->serial);
    transport->serial = NULL;
    transport->open = false;
}

bool updi_transport_recv(UpdiTransport* transport, uint8_t* data, size_t size, uint32_t timeout_ms) {
    size_t got = 0;
    uint32_t start = furi_get_tick();
    uint32_t limit = furi_ms_to_ticks(timeout_ms);
    while(got < size) {
        uint32_t elapsed = furi_get_tick() - start;
        if(elapsed >= limit) {
            updi_transport_set_error(transport, "No response (timeout)");
            return false;
        }
        size_t nread =
            furi_stream_buffer_receive(transport->rx, data + got, size - got, limit - elapsed);
        if(nread == 0) {
            updi_transport_set_error(transport, "No response (timeout)");
            return false;
        }
        got += nread;
    }
    return true;
}

bool updi_transport_send_raw(UpdiTransport* transport, const uint8_t* data, size_t size) {
    if(!transport->open || data == NULL) {
        updi_transport_set_error(transport, "UART is closed");
        return false;
    }
    furi_hal_serial_tx(transport->serial, data, size);
    furi_hal_serial_tx_wait_complete(transport->serial);
    return true;
}

bool updi_transport_send(UpdiTransport* transport, const uint8_t* data, size_t size) {
    if(!updi_transport_send_raw(transport, data, size)) return false;
    uint8_t echo[32];
    size_t left = size;
    while(left > 0) {
        size_t chunk = left > sizeof(echo) ? sizeof(echo) : left;
        if(!updi_transport_recv(transport, echo, chunk, 200)) return false;
        left -= chunk;
    }
    return true;
}

size_t updi_transport_recv_available(
    UpdiTransport* transport,
    uint8_t* data,
    size_t max_size,
    uint32_t timeout_ms) {
    if(!transport->open || data == NULL || max_size == 0) return 0;
    return furi_stream_buffer_receive(transport->rx, data, max_size, furi_ms_to_ticks(timeout_ms));
}

bool updi_transport_is_open(const UpdiTransport* transport) {
    return transport != NULL && transport->open;
}

void updi_transport_flush_rx(UpdiTransport* transport) {
    uint8_t scratch[32];
    while(furi_stream_buffer_receive(transport->rx, scratch, sizeof(scratch), 0) > 0) {
    }
}

bool updi_transport_set_baud(UpdiTransport* transport, uint32_t baud) {
    if(!transport->open) {
        updi_transport_set_error(transport, "UART is closed");
        return false;
    }
    if(!furi_hal_serial_is_baud_rate_supported(transport->serial, baud)) {
        updi_transport_set_error(transport, "Baud rate not supported");
        return false;
    }
    furi_hal_serial_tx_wait_complete(transport->serial);
    furi_hal_serial_async_rx_stop(transport->serial);
    furi_hal_serial_set_br(transport->serial, baud);
    updi_transport_apply_framing(transport);
    transport->baud = baud;
    furi_stream_buffer_reset(transport->rx);
    furi_hal_serial_async_rx_start(transport->serial, updi_rx_callback, transport, false);
    furi_delay_ms(2);
    updi_transport_flush_rx(transport);
    return true;
}

static void updi_drive_break(UpdiTransport* transport) {
    furi_hal_serial_tx_wait_complete(transport->serial);
    furi_hal_serial_async_rx_stop(transport->serial);
    furi_hal_serial_disable_direction(transport->serial, FuriHalSerialDirectionTx);

    const GpioPin* tx = furi_hal_serial_get_gpio_pin(transport->serial, FuriHalSerialDirectionTx);
    furi_hal_gpio_init_simple(tx, GpioModeOutputPushPull);

    /* Two lows of at least 24.6 ms. A BREAK resets the UPDI clock to 4 MHz. */
    furi_hal_gpio_write(tx, false);
    furi_delay_ms(25);
    furi_hal_gpio_write(tx, true);
    furi_delay_ms(10);
    furi_hal_gpio_write(tx, false);
    furi_delay_ms(25);
    furi_hal_gpio_write(tx, true);
    furi_delay_ms(2);

    furi_hal_serial_enable_direction(transport->serial, FuriHalSerialDirectionTx);
    updi_transport_apply_framing(transport);
    furi_stream_buffer_reset(transport->rx);
    furi_hal_serial_async_rx_start(transport->serial, updi_rx_callback, transport, false);
    furi_delay_ms(5);
    updi_transport_flush_rx(transport);
}

bool updi_transport_double_break(UpdiTransport* transport) {
    if(!transport->open) {
        updi_transport_set_error(transport, "UART is closed");
        return false;
    }
    if(transport->baud != UPDI_BAUD_SAFE) {
        if(!updi_transport_set_baud(transport, UPDI_BAUD_SAFE)) return false;
    }
    updi_drive_break(transport);
    return true;
}

const char* updi_transport_error(const UpdiTransport* transport) {
    return transport->error;
}

uint32_t updi_transport_baud(const UpdiTransport* transport) {
    return transport->baud;
}
