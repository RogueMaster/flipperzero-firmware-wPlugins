/* Transport over the Flipper's USB CDC port (interface 0).
 *
 * open() takes the port from the CLI and switches USB to a single CDC
 * interface; close() hands it back and restores the previous USB mode.
 * Received bytes are framed into lines of up to PROTO_LINE_MAX - 1 bytes on
 * a worker thread, which also calls the rx callback.
 */
#pragma once

#include "transport.h"

Transport* transport_usb_alloc(void);

/* Closes the transport if needed. */
void transport_usb_free(Transport* transport);
