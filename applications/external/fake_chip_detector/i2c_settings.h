#pragma once

#include <stdint.h>
#include <stdbool.h>

// Bumped whenever the struct layout changes; an older file is discarded
// rather than misread.
#define I2C_SETTINGS_MAGIC   0x1C
#define I2C_SETTINGS_VERSION 1

typedef struct {
    bool sound; // melodies on verdicts
    bool vibro; // haptic feedback
    bool led; // RGB notification LED
    bool backlight; // keep the display lit during long scans
    uint8_t probe_timeout_idx; // index into i2c_settings_timeout_values
    bool autosave; // write a log to SD after every scan
    bool deep_probe; // read WHO_AM_I candidates at unknown addresses too
} I2CSettings;

extern const uint32_t i2c_settings_timeout_values[];
extern const char* const i2c_settings_timeout_names[];
#define I2C_SETTINGS_TIMEOUT_COUNT 3

void i2c_settings_load(I2CSettings* settings);
void i2c_settings_save(const I2CSettings* settings);
uint32_t i2c_settings_probe_timeout(const I2CSettings* settings);
