/* Transport over the FliBuddy BLE profile (ble_flibuddy_svc.h).
 *
 * open() replaces the Flipper's default BLE profile with FliBuddy's;
 * close() disconnects the host and restores the default one, so the
 * official Flipper app cannot connect while the transport is open. A host
 * is connected once it subscribed to TX, as DTR on USB (see ble_wire_link
 * for why the Bt status does not say so). Received bytes are framed into
 * lines of up to PROTO_LINE_MAX - 1 bytes on a worker thread, which also
 * calls the rx callback.
 */
#pragma once

#include "transport.h"

typedef enum {
    TransportBleOff, /* not advertising: Bluetooth is off or unavailable */
    TransportBleAdvertising,
    TransportBleConnected, /* a host subscribed to TX; it may not have said hello */
} TransportBleStatus;

Transport* transport_ble_alloc(void);

/* Closes the transport if needed. */
void transport_ble_free(Transport* transport);

/* For the status line. */
TransportBleStatus transport_ble_status(const Transport* transport);
