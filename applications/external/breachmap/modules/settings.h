#pragma once

#include "../models/recon_types.h"
#include <storage/storage.h>

/* Lightweight screen-lock settings. The PIN is stored only as a hash; this is an
 * access gate for casual protection, not at-rest encryption of the .recon files. */

/* Non-cryptographic hash of a PIN string (never returns 0). */
uint32_t breach_pin_hash(const char* pin);

/* Load the stored PIN hash. Sets *pin_set to whether a PIN is configured. */
void breach_settings_load(Storage* storage, uint32_t* pin_hash, bool* pin_set);

/* Persist the PIN hash and whether a PIN is set. */
bool breach_settings_save(Storage* storage, uint32_t pin_hash, bool pin_set);
