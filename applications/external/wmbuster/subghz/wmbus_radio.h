#pragma once
/* Thin wrapper around `subghz_devices_*` that picks the internal
 * CC1101 or a GPIO-attached external module and manages the OTG rail.
 * Falls back to the internal radio if the external module isn't found. */

#include <lib/subghz/devices/devices.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WmbusModuleInternal = 0,
    WmbusModuleExternal = 1,
    WmbusModule_Count,
} WmbusModule;

const SubGhzDevice* wmbus_radio_select(
    const SubGhzDevice* current,
    WmbusModule module);

bool wmbus_radio_is_external(const SubGhzDevice* dev);

void wmbus_radio_release(const SubGhzDevice* dev);

#ifdef __cplusplus
}
#endif
