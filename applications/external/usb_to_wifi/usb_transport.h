#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UsbTransport UsbTransport;

typedef enum {
    UsbTransportEventUsbConnected,
    UsbTransportEventUsbDisconnected,
    UsbTransportEventPortOpened,
    UsbTransportEventPortClosed,
    UsbTransportEventRxOverflow,
} UsbTransportEvent;

typedef void (*UsbTransportReceiveCallback)(const uint8_t* data, size_t length, void* context);
typedef void (*UsbTransportEventCallback)(UsbTransportEvent event, void* context);

UsbTransport* usb_transport_alloc(
    UsbTransportReceiveCallback receive_callback,
    UsbTransportEventCallback event_callback,
    void* callback_context);
void usb_transport_free(UsbTransport* transport);

bool usb_transport_start(UsbTransport* transport);
void usb_transport_stop(UsbTransport* transport);

/* deadline_tick is an absolute furi_get_tick() deadline shared by the whole frame. */
bool usb_transport_send_until(
    UsbTransport* transport,
    const uint8_t* data,
    size_t length,
    uint32_t deadline_tick);

bool usb_transport_is_usb_connected(UsbTransport* transport);
bool usb_transport_is_port_open(UsbTransport* transport);

#ifdef __cplusplus
}
#endif
