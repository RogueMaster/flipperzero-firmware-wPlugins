#pragma once

#include <stdbool.h>

#include "seos.h"

/* Settings that outlive a run.
 *
 * There is one so far: whether the external BLE dongle is in use. It is a
 * setting rather than something probed for, because its stack is loaded only
 * when it is turned on, and the user is the one who knows whether a dongle is
 * attached.
 */

/* Reads the settings file, leaving defaults in place if there is not one. */
void seos_settings_load(Seos* seos);

/* Writes the settings file. */
bool seos_settings_save(Seos* seos);
