#include <furi_hal_vibro.h>
#include <furi_hal_gpio.h>
#include <cfw/cfw.h>
#include <rgb_backlight.h>

#define TAG "FuriHalVibro"

void furi_hal_vibro_init(void) {
    furi_hal_gpio_init(&gpio_vibro, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_vibro, false);
    FURI_LOG_I(TAG, "Init OK");
}

void furi_hal_vibro_on(bool value) {
    furi_hal_gpio_write(&gpio_vibro, value);
    // On RGB-backlight-modded devices the SK6805 LED data line shares pin PA8
    // with the vibro motor (VIBRO_Pin == SK6805 led_pin). A vibro pulse drives
    // that line for tens of ms, shifting garbage bits into the LED chain that
    // latch as wrong colors (issue #541). Re-apply the configured colors once
    // the pulse ends. No-op on devices without the RGB backlight setting.
    if(!value && cfw_settings.rgb_backlight) {
        rgb_backlight_reconfigure(cfw_settings.rgb_backlight);
    }
}
