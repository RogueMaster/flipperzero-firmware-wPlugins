#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FLIPPER_BLINKER_WPM_MIN              (5U)
#define FLIPPER_BLINKER_WPM_MAX              (40U)
#define FLIPPER_BLINKER_WPM_DEFAULT          (15U)
#define FLIPPER_BLINKER_REPEAT_DELAY_MIN     (1U)
#define FLIPPER_BLINKER_REPEAT_DELAY_MAX     (300U)
#define FLIPPER_BLINKER_REPEAT_DELAY_DEFAULT (5U)

typedef enum {
    FlipperBlinkerLedPinPc3,
    FlipperBlinkerLedPinCount,
} FlipperBlinkerLedPin;

typedef struct {
    uint8_t wpm;
    bool repeat;
    uint16_t repeat_delay_seconds;
    FlipperBlinkerLedPin led_pin;
} FlipperBlinkerSettings;

void flipper_blinker_settings_set_defaults(FlipperBlinkerSettings* settings);
void flipper_blinker_settings_load(FlipperBlinkerSettings* settings);
void flipper_blinker_settings_save(const FlipperBlinkerSettings* settings);
const char* flipper_blinker_settings_pin_name(FlipperBlinkerLedPin pin);
