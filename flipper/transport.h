#pragma once
#include <furi.h>
#include "link.h"

typedef struct PhrTransport PhrTransport;

typedef struct {
    const char* name; // "Bluetooth LE" / "USB CDC"
    uint8_t wire_id; // value for HELLO.transport (0 = BLE, 1 = USB)
    PhrTransport* (*start)(PhrLink* link);
    void (*stop)(PhrTransport* t);
    /** Called from the UI tick; sends HELLO when the link says it is due. */
    void (*tick)(PhrTransport* t);
    /** One-line status for the waiting screen, e.g. "Advertising". */
    const char* (*status)(PhrTransport* t);
    /** Advertised / device name to show to the user. */
    const char* (*device_name)(PhrTransport* t);
} PhrTransportApi;

extern const PhrTransportApi phr_transport_ble;
extern const PhrTransportApi phr_transport_usb;

static inline const PhrTransportApi* phr_transport_by_id(uint8_t id) {
    return id == 1 ? &phr_transport_usb : &phr_transport_ble;
}
