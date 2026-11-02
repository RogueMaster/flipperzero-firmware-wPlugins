#pragma once

#include <stdint.h>

#include "settings.h"

void flipper_blinker_gpio_init(FlipperBlinkerLedPin pin);
void flipper_blinker_gpio_deinit(FlipperBlinkerLedPin pin);
void led_on(void);
void led_off(void);
void blink_dot(uint32_t unit_ms);
void blink_dash(uint32_t unit_ms);
