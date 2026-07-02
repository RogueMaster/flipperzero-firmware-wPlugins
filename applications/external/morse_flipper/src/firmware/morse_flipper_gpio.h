/*
 * Purpose: Publish logical GPIO pin IDs and conflict checks.
 * Owns: Morse GPIO pin enum, names, defaults, and pin pair validation API.
 * Depends on: Flipper GPIO types only in FAP builds.
 * Tests: tests/test_gpio.c.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MORSE_FLIPPER_GPIO_PIN_COUNT 8U
#define MORSE_FLIPPER_GPIO_PIN_NONE  0xFFU

typedef enum {
    MorseFlipperGpioPinP2 = 0,
    MorseFlipperGpioPinP3,
    MorseFlipperGpioPinP4,
    MorseFlipperGpioPinP5,
    MorseFlipperGpioPinP6,
    MorseFlipperGpioPinP7,
    MorseFlipperGpioPinP15,
    MorseFlipperGpioPinP16,
} MorseFlipperGpioPinChoice;

typedef enum {
    MorseFlipperGpioRuleOk = 0,
    MorseFlipperGpioRuleBadIndex,
    MorseFlipperGpioRulePaddlesSharePin,
    MorseFlipperGpioRuleGroundShared,
} MorseFlipperGpioRule;

bool morse_flipper_gpio_pin_valid(uint8_t pin_idx);
const char* morse_flipper_gpio_name(uint8_t pin_idx);

uint8_t morse_flipper_gpio_default_dit(void);
uint8_t morse_flipper_gpio_default_dah(void);
uint8_t morse_flipper_gpio_default_ground(void);

MorseFlipperGpioRule morse_flipper_gpio_validate(uint8_t dit, uint8_t dah, uint8_t ground);

const char* morse_flipper_gpio_rule_text(MorseFlipperGpioRule rule);
