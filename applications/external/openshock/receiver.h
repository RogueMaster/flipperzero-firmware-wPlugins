#pragma once

#include "protocols.h"
#include <stdbool.h>

typedef struct OpenshockRx OpenshockRx;

// Allocate the receiver.
OpenshockRx* openshock_rx_alloc(void);

// Free the receiver.
void openshock_rx_free(OpenshockRx* rx);

// Start listening for OOK transmissions at 433.92 MHz.
void openshock_rx_start(OpenshockRx* rx);

// Stop listening.
void openshock_rx_stop(OpenshockRx* rx);

// Check if a decoded result is available. If so, copies it to *result and returns true.
// Clears the result after reading.
bool openshock_rx_get_result(OpenshockRx* rx, DecodedShocker* result);
