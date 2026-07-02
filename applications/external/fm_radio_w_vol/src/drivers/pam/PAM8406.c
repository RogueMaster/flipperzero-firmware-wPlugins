#include "PAM8406.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

static const GpioPin* pam8406_mute_pin = &gpio_ext_pb3;
static const GpioPin* pam8406_mode_pin = &gpio_ext_pb2;
static const GpioPin* pam8406_shutdown_pin = &gpio_ext_pc3;
static bool pam8406_powered = false;

static void pam8406_write_safe_defaults(void) {
    furi_hal_gpio_write(pam8406_mute_pin, false);
    furi_hal_gpio_write(pam8406_mode_pin, false);
    furi_hal_gpio_write(pam8406_shutdown_pin, false);
    pam8406_powered = false;
}

void pam8406_init(void) {
    furi_hal_gpio_init(pam8406_mute_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(pam8406_mode_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(pam8406_shutdown_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    pam8406_write_safe_defaults();
}

void pam8406_apply_state(const PAM8406State* state) {
    if(!state) return;

    if(state->powered) {
        furi_hal_gpio_write(pam8406_mode_pin, state->class_d_mode);
        furi_hal_gpio_write(pam8406_shutdown_pin, true);
        if(!pam8406_powered) {
            furi_delay_ms(20);
        }
        furi_hal_gpio_write(pam8406_mute_pin, !state->muted);
        pam8406_powered = true;
    } else {
        furi_hal_gpio_write(pam8406_mute_pin, false);
        if(pam8406_powered) {
            furi_delay_ms(5);
        }
        furi_hal_gpio_write(pam8406_shutdown_pin, false);
        furi_hal_gpio_write(pam8406_mode_pin, state->class_d_mode);
        pam8406_powered = false;
    }
}

void pam8406_shutdown(void) {
    furi_hal_gpio_write(pam8406_mute_pin, false);
    furi_delay_ms(5);
    furi_hal_gpio_write(pam8406_shutdown_pin, false);
    furi_hal_gpio_write(pam8406_mode_pin, false);
    pam8406_powered = false;
}
