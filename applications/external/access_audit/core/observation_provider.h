#pragma once

#include <stdbool.h>
#include <nfc/nfc.h>
#include "observation.h"

/**
 * Stateful NFC observation provider.
 *
 * Lifecycle:
 *   alloc → start → poll (in main loop) → stop → free
 *
 * poll() drives the internal state machine (Scanner → Poller → result).
 * Returns true exactly once per card read when a valid observation is ready.
 * After returning true the provider goes idle; call start() again to resume.
 */
typedef struct ObservationProvider ObservationProvider;

ObservationProvider* observation_provider_alloc(void);
void observation_provider_free(ObservationProvider* provider);

/** Begin scanning. No-op if already active. */
void observation_provider_start(ObservationProvider* provider);

/** Stop any active scan and release NFC hardware. Safe to call at any time. */
void observation_provider_stop(ObservationProvider* provider);

/**
 * Drive the state machine. Call from the main app loop.
 *
 * Returns true when a valid observation has been obtained; *out is filled.
 * After returning true the provider is idle (NFC hardware released).
 * Returns false on every other call.
 */
bool observation_provider_poll(ObservationProvider* provider, AccessObservation* out);

/**
 * Returns the underlying Nfc* instance.
 * Only borrow this when the provider is stopped — used by IclassProvider to
 * share a single NFC hardware handle and avoid hardware contention.
 */
Nfc* observation_provider_get_nfc(ObservationProvider* provider);

