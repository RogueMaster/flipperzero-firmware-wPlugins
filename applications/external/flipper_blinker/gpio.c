#include "gpio.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>

static const GpioPin* active_led_pin = &gpio_ext_pc3;

static const GpioPin* flipper_blinker_gpio_get_pin(FlipperBlinkerLedPin pin) {
    if(pin == FlipperBlinkerLedPinPc3) {
        return &gpio_ext_pc3;
    }
    return &gpio_ext_pc3;
}

void flipper_blinker_gpio_init(FlipperBlinkerLedPin pin) {
    active_led_pin = flipper_blinker_gpio_get_pin(pin);
    furi_hal_gpio_init(active_led_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    led_off();
}

void flipper_blinker_gpio_deinit(FlipperBlinkerLedPin pin) {
    const GpioPin* gpio = flipper_blinker_gpio_get_pin(pin);
    furi_hal_gpio_write(gpio, false);
    furi_hal_gpio_init(gpio, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void led_on(void) {
    furi_hal_gpio_write(active_led_pin, true);
}

void led_off(void) {
    furi_hal_gpio_write(active_led_pin, false);
}

void blink_dot(uint32_t unit_ms) {
    led_on();
    furi_delay_ms(unit_ms);
    led_off();
}

void blink_dash(uint32_t unit_ms) {
    led_on();
    furi_delay_ms(unit_ms * 3U);
    led_off();
}
