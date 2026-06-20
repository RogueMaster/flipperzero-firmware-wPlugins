#pragma once

#include <stdbool.h>
#include <nfc/nfc.h>
#include "observation.h"

/**
 * HID iCLASS provider.
 *
 * HID iCLASS does not respond to standard ISO15693 inventory commands, so it
 * is invisible to NfcScanner. This provider uses the raw NFC API with the
 * iCLASS-specific ACTALL (0x0A) and IDENTIFY (0x0C) commands to obtain the
 * CSN (card serial number, equivalent to a UID).
 *
 * The Nfc* instance is BORROWED — the caller owns it and must stop the
 * ObservationProvider before starting this provider, to prevent hardware
 * contention (both use the same 13.56 MHz NFC hardware).
 *
 * Protocol reference: HID iCLASS ACTALL/IDENTIFY command sequence
 */

typedef struct IclassProvider IclassProvider;

/** nfc is borrowed; stop ObservationProvider before calling iclass_provider_start(). */
IclassProvider* iclass_provider_alloc(Nfc* nfc);
void iclass_provider_free(IclassProvider* provider);

void iclass_provider_start(IclassProvider* provider);
void iclass_provider_stop(IclassProvider* provider);

/** Returns true and fills *out once a card has been identified. */
bool iclass_provider_poll(IclassProvider* provider, AccessObservation* out);
