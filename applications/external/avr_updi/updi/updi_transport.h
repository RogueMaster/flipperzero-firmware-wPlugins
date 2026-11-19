#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    UpdiPins15_16 = 0,
    UpdiPins13_14 = 1,
} UpdiPins;

typedef struct UpdiTransport UpdiTransport;

UpdiTransport* updi_transport_alloc(void);
void updi_transport_free(UpdiTransport* transport);

void updi_transport_set_pins(UpdiTransport* transport, UpdiPins pins);
bool updi_transport_open(UpdiTransport* transport);
void updi_transport_close(UpdiTransport* transport);

bool updi_transport_send(UpdiTransport* transport, const uint8_t* data, size_t size);
/** TX without discarding local echo (USB SerialUPDI bridge). */
bool updi_transport_send_raw(UpdiTransport* transport, const uint8_t* data, size_t size);
bool updi_transport_recv(UpdiTransport* transport, uint8_t* data, size_t size, uint32_t timeout_ms);
size_t updi_transport_recv_available(
    UpdiTransport* transport,
    uint8_t* data,
    size_t max_size,
    uint32_t timeout_ms);

void updi_transport_flush_rx(UpdiTransport* transport);
bool updi_transport_double_break(UpdiTransport* transport);
bool updi_transport_set_baud(UpdiTransport* transport, uint32_t baud);
bool updi_transport_is_open(const UpdiTransport* transport);

const char* updi_transport_error(const UpdiTransport* transport);
uint32_t updi_transport_baud(const UpdiTransport* transport);
