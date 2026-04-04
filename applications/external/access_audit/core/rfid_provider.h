#pragma once

#include <stdbool.h>
#include "observation.h"

typedef struct RfidProvider RfidProvider;

RfidProvider* rfid_provider_alloc(void);
void rfid_provider_free(RfidProvider* provider);

/** Begin continuous RFID scanning. */
void rfid_provider_start(RfidProvider* provider);

/** Stop scanning. Safe to call even if not started. */
void rfid_provider_stop(RfidProvider* provider);

/**
 * Check for a completed read.
 * Returns true and fills *out when a card has been decoded since the last
 * successful poll. Returns false otherwise (call again next tick).
 */
bool rfid_provider_poll(RfidProvider* provider, AccessObservation* out);
