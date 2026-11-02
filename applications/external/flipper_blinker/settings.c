#include "settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define FLIPPER_BLINKER_SETTINGS_PATH    APP_DATA_PATH("flipper_blinker.settings")
#define FLIPPER_BLINKER_SETTINGS_MAGIC   (0xB1U)
#define FLIPPER_BLINKER_SETTINGS_VERSION (1U)

static void flipper_blinker_settings_sanitize(FlipperBlinkerSettings* settings) {
    if(settings->wpm < FLIPPER_BLINKER_WPM_MIN) settings->wpm = FLIPPER_BLINKER_WPM_MIN;
    if(settings->wpm > FLIPPER_BLINKER_WPM_MAX) settings->wpm = FLIPPER_BLINKER_WPM_MAX;
    if(settings->repeat_delay_seconds < FLIPPER_BLINKER_REPEAT_DELAY_MIN) {
        settings->repeat_delay_seconds = FLIPPER_BLINKER_REPEAT_DELAY_MIN;
    }
    if(settings->repeat_delay_seconds > FLIPPER_BLINKER_REPEAT_DELAY_MAX) {
        settings->repeat_delay_seconds = FLIPPER_BLINKER_REPEAT_DELAY_MAX;
    }
    if(settings->led_pin >= FlipperBlinkerLedPinCount) {
        settings->led_pin = FlipperBlinkerLedPinPc3;
    }
}

void flipper_blinker_settings_set_defaults(FlipperBlinkerSettings* settings) {
    furi_assert(settings);
    settings->wpm = FLIPPER_BLINKER_WPM_DEFAULT;
    settings->repeat = false;
    settings->repeat_delay_seconds = FLIPPER_BLINKER_REPEAT_DELAY_DEFAULT;
    settings->led_pin = FlipperBlinkerLedPinPc3;
}

void flipper_blinker_settings_load(FlipperBlinkerSettings* settings) {
    furi_assert(settings);
    flipper_blinker_settings_set_defaults(settings);
    saved_struct_load(
        FLIPPER_BLINKER_SETTINGS_PATH,
        settings,
        sizeof(FlipperBlinkerSettings),
        FLIPPER_BLINKER_SETTINGS_MAGIC,
        FLIPPER_BLINKER_SETTINGS_VERSION);
    flipper_blinker_settings_sanitize(settings);
}

void flipper_blinker_settings_save(const FlipperBlinkerSettings* settings) {
    furi_assert(settings);
    saved_struct_save(
        FLIPPER_BLINKER_SETTINGS_PATH,
        settings,
        sizeof(FlipperBlinkerSettings),
        FLIPPER_BLINKER_SETTINGS_MAGIC,
        FLIPPER_BLINKER_SETTINGS_VERSION);
}

const char* flipper_blinker_settings_pin_name(FlipperBlinkerLedPin pin) {
    if(pin == FlipperBlinkerLedPinPc3) {
        return "PC3";
    }
    return "Unknown";
}
