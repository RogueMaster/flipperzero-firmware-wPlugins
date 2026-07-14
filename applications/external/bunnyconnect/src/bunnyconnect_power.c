#include "../lib/bunnyconnect_power.h"
#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>

static bool usb_power_enabled = false;

void bunnyconnect_power_init(void) {
    FURI_LOG_I("BunnyPower", "Initializing USB CDC for power and communication");
    usb_power_enabled = false;
}

void bunnyconnect_power_deinit(void) {
    bunnyconnect_power_set_usb_enabled(false);
    FURI_LOG_I("BunnyPower", "USB power and communication disabled");
}

bool bunnyconnect_power_set_usb_enabled(bool enabled) {
    if(enabled) {
        // Enable USB device mode to provide power through USB port
        furi_hal_usb_unlock();

        // Set to CDC mode for serial communication
        if(furi_hal_usb_set_config(&usb_cdc_single, NULL)) {
            usb_power_enabled = true;
            FURI_LOG_I("BunnyPower", "USB CDC enabled - providing power and serial communication");

            // Wait for configuration to be applied
            furi_delay_ms(500);
            return true;
        } else {
            FURI_LOG_E("BunnyPower", "Failed to enable USB CDC mode");
            return false;
        }
    } else {
        // Disable USB to cut power
        furi_hal_usb_disable();
        usb_power_enabled = false;
        FURI_LOG_I("BunnyPower", "USB power disabled");
        return true;
    }
}

bool bunnyconnect_power_is_usb_enabled(void) {
    return usb_power_enabled;
}

bool bunnyconnect_power_is_usb_connected(void) {
    // Check if USB device is connected and configured
    return furi_hal_usb_get_config() != NULL && usb_power_enabled;
}
