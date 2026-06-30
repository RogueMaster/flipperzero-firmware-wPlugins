// helpers/radio_device_loader.c
#include "radio_device_loader.h"

#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <furi.h>
#include <furi_hal.h>
#include "../defines.h"

#define TAG "RadioDeviceLoader"

static bool radio_device_loader_otg_enabled_by_loader = false;

static void radio_device_loader_power_on() {
    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        // CC1101 power-up time
        furi_delay_ms(10);
    }
    if(furi_hal_power_is_otg_enabled()) {
        radio_device_loader_otg_enabled_by_loader = true;
    }
    FURI_LOG_D(TAG, "OTG power enabled after %d attempts", attempts);
}

static void radio_device_loader_power_off() {
    if(radio_device_loader_otg_enabled_by_loader && furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
        radio_device_loader_otg_enabled_by_loader = false;
        FURI_LOG_D(TAG, "OTG power disabled");
    }
}

bool radio_device_loader_is_connect_external(const char* name) {
    bool is_connect = false;
    bool is_otg_enabled = furi_hal_power_is_otg_enabled();

    if(!is_otg_enabled) {
        radio_device_loader_power_on();
    }

    const SubGhzDevice* device = subghz_devices_get_by_name(name);
    if(device) {
        is_connect = subghz_devices_is_connect(device);
        FURI_LOG_D(TAG, "External device '%s' connect check: %s", name, is_connect ? "YES" : "NO");
    } else {
        FURI_LOG_W(TAG, "Could not get device by name: %s", name);
    }

    if(!is_otg_enabled) {
        radio_device_loader_power_off();
    }
    return is_connect;
}

const SubGhzDevice* radio_device_loader_set(
    const SubGhzDevice* current_radio_device,
    SubGhzRadioDeviceType radio_device_type) {
    const SubGhzDevice* target_radio_device = NULL;

    // Decide the target device first (external if requested+present, else internal)
    if(radio_device_type == SubGhzRadioDeviceTypeExternalCC1101 &&
       radio_device_loader_is_connect_external(SUBGHZ_DEVICE_CC1101_EXT_NAME)) {
        radio_device_loader_power_on();
        target_radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
        if(!target_radio_device) {
            FURI_LOG_E(TAG, "Failed to get external CC1101 device, falling back to internal");
        }
    }

    if(!target_radio_device) {
        target_radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        if(!target_radio_device) {
            FURI_LOG_E(TAG, "Failed to get internal CC1101 device");
            return NULL;
        }
    }

    // If we’re already on the target device, don’t reload
    if(current_radio_device == target_radio_device) {
        if(target_radio_device == subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME)) {
            FURI_LOG_I(TAG, "External CC1101 already selected");
        } else {
            FURI_LOG_I(TAG, "Internal CC1101 already selected");
        }
        return target_radio_device;
    }

    // Cleanly stop the current device before switching
    if(current_radio_device) {
        radio_device_loader_end(current_radio_device);
    }

    // Start the target device
    subghz_devices_begin(target_radio_device);

    // Log what we ended up with
    if(target_radio_device == subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME)) {
        FURI_LOG_I(TAG, "Switched to external CC1101");
    } else {
        if(radio_device_type == SubGhzRadioDeviceTypeExternalCC1101) {
            FURI_LOG_I(TAG, "External requested but unavailable; switched to internal CC1101");
        } else {
            FURI_LOG_I(TAG, "Switched to internal CC1101");
        }
    }

    return target_radio_device;
}

bool radio_device_loader_is_external(const SubGhzDevice* radio_device) {
    if(!radio_device) {
        FURI_LOG_W(TAG, "is_external called with NULL device");
        return false;
    }

    const SubGhzDevice* internal_device =
        subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    bool is_external = (radio_device != internal_device);

    FURI_LOG_D(
        TAG,
        "is_external check: device=%p, internal=%p, result=%s",
        radio_device,
        internal_device,
        is_external ? "EXTERNAL" : "INTERNAL");

    return is_external;
}

void radio_device_loader_end(const SubGhzDevice* radio_device) {
    furi_check(radio_device);

    if(radio_device != subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME)) {
        subghz_devices_end(radio_device);
        FURI_LOG_I(TAG, "External radio device ended");
    } else {
        FURI_LOG_D(TAG, "Internal radio device - no cleanup needed");
    }
    radio_device_loader_power_off();
}
