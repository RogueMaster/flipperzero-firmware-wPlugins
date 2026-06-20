#pragma once

#include "protocols.h"
#include <stdbool.h>

#define OPENSHOCK_FREQUENCY 433920000

// Opaque transmitter handle.
typedef struct OpenshockTx OpenshockTx;

// Allocate the transmitter (creates a background thread, but does not transmit yet).
OpenshockTx* openshock_tx_alloc(void);

// Free the transmitter and its background thread.
void openshock_tx_free(OpenshockTx* tx);

// Start transmitting a pulse sequence continuously. Non-blocking.
// Copies the pulse data internally. Returns true on success.
bool openshock_tx_start(OpenshockTx* tx, const OokPulse* pulses, size_t count);

// Stop the current transmission. Non-blocking (signals stop, teardown happens in background).
void openshock_tx_stop(OpenshockTx* tx);

// Check if currently transmitting.
bool openshock_tx_is_active(OpenshockTx* tx);
