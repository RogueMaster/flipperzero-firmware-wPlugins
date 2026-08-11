#include "i2c_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define SETTINGS_PATH APP_DATA_PATH("settings.bin")

// Slower buses and long jumpers need a longer probe window; the fast setting
// keeps a full 0x08..0x77 sweep under a second.
const uint32_t i2c_settings_timeout_values[] = {5, 10, 20};
const char* const i2c_settings_timeout_names[] = {"Fast 5ms", "Normal 10ms", "Slow 20ms"};

static void i2c_settings_set_defaults(I2CSettings* settings) {
    settings->sound = true;
    settings->vibro = true;
    settings->led = true;
    settings->backlight = true;
    settings->probe_timeout_idx = 1;
    settings->autosave = false;
    settings->deep_probe = true;
}

void i2c_settings_load(I2CSettings* settings) {
    if(!saved_struct_load(
           SETTINGS_PATH, settings, sizeof(I2CSettings), I2C_SETTINGS_MAGIC, I2C_SETTINGS_VERSION)) {
        i2c_settings_set_defaults(settings);
        return;
    }
    // A corrupt or hand-edited file must not push the timeout index past the
    // end of the lookup table.
    if(settings->probe_timeout_idx >= I2C_SETTINGS_TIMEOUT_COUNT) {
        settings->probe_timeout_idx = 1;
    }
}

void i2c_settings_save(const I2CSettings* settings) {
    // saved_struct_save creates the app data directory as needed; a failure
    // here only costs persistence, so it is not surfaced to the user.
    saved_struct_save(
        SETTINGS_PATH,
        (void*)settings,
        sizeof(I2CSettings),
        I2C_SETTINGS_MAGIC,
        I2C_SETTINGS_VERSION);
}

uint32_t i2c_settings_probe_timeout(const I2CSettings* settings) {
    uint8_t idx = settings->probe_timeout_idx;
    if(idx >= I2C_SETTINGS_TIMEOUT_COUNT) idx = 1;
    return i2c_settings_timeout_values[idx];
}
