#include "wmbus_radio.h"
/* Selects the active CC1101 (internal/external), powers the GPIO 5 V
 * rail when the external module is in use, releases it on teardown.
 * Modeled on ProtoPirate's radio_device_loader. */

#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>

#define TAG "WmbusRadio"

/* Only release the OTG rail if we were the one who enabled it. */
static bool s_otg_owned = false;

static void otg_power_on(void) {
    uint8_t tries = 0;
    while(!furi_hal_power_is_otg_enabled() && tries++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    if(furi_hal_power_is_otg_enabled()) s_otg_owned = true;
}

static void otg_power_off(void) {
    if(s_otg_owned && furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
        s_otg_owned = false;
    }
}

static bool external_is_connected(void) {
    bool was_on = furi_hal_power_is_otg_enabled();
    if(!was_on) otg_power_on();

    bool present = false;
    const SubGhzDevice* d =
        subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
    if(d) present = subghz_devices_is_connect(d);

    if(!was_on) otg_power_off();
    return present;
}

const SubGhzDevice* wmbus_radio_select(
    const SubGhzDevice* current,
    WmbusModule module) {

    const SubGhzDevice* target = NULL;
    if(module == WmbusModuleExternal && external_is_connected()) {
        otg_power_on();
        target = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
        if(!target) {
            FURI_LOG_E(TAG, "external plugin missing, fallback to internal");
        }
    }
    if(!target) {
        target = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        if(!target) return NULL; /* should be impossible */
    }

    if(current == target) return target;

    if(current) wmbus_radio_release(current);
    subghz_devices_begin(target);
    return target;
}

bool wmbus_radio_is_external(const SubGhzDevice* dev) {
    if(!dev) return false;
    const SubGhzDevice* internal =
        subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    return dev != internal;
}

void wmbus_radio_release(const SubGhzDevice* dev) {
    if(!dev) return;
    const SubGhzDevice* internal =
        subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(dev != internal) {
        subghz_devices_end(dev);
    }
    otg_power_off();
}
