#pragma once
/* Thin wrapper around `subghz_devices_*` that picks the internal
 * CC1101 or a GPIO-attached external module and manages the OTG rail.
 * Falls back to the internal radio if the external module isn't found. */

#include <lib/subghz/devices/devices.h>
#include "../wmbus_app_i.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Probe GPIO SPI for an external CC1101 (same detection as the
 * Flux Capacitor / Sub-GHz app: OTG power + subghz_devices_is_connect).
 * Returns true if a working external CC1101 answers. */
bool wmbus_radio_detect_external(void);

/* Select the active radio.  Handles Auto / Internal / External:
 *   Auto     – probes external first, falls back to internal.
 *   Internal – always internal.
 *   External – attempts external, falls back to internal if absent. */
const SubGhzDevice* wmbus_radio_select(const SubGhzDevice* current, WmbusModuleSetting module);

bool wmbus_radio_is_external(const SubGhzDevice* dev);

void wmbus_radio_release(const SubGhzDevice* dev);

#ifdef __cplusplus
}
#endif
