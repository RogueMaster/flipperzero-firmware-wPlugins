#include "seos_settings.h"

#include "seos_i.h"

#include <lib/flipper_format/flipper_format.h>

#define TAG "SeosSettings"

#define SETTINGS_PATH    APP_DATA_PATH("settings")
#define SETTINGS_HEADER  "Flipper Seos Settings"
#define SETTINGS_VERSION 1

#define KEY_EXTERNAL_BLE "External BLE"

void seos_settings_load(Seos* seos) {
    furi_assert(seos);

    FlipperFormat* file = flipper_format_file_alloc(seos->credential->storage);
    FuriString* header = furi_string_alloc();
    uint32_t version = 0;
    uint32_t external_ble = 0;

    do {
        if(!flipper_format_file_open_existing(file, SETTINGS_PATH)) break;
        if(!flipper_format_read_header(file, header, &version)) break;
        if(!furi_string_equal_str(header, SETTINGS_HEADER) || version != SETTINGS_VERSION) {
            FURI_LOG_W(TAG, "Settings file is not one of ours");
            break;
        }
        if(!flipper_format_read_uint32(file, KEY_EXTERNAL_BLE, &external_ble, 1)) break;

        seos->has_external_ble = external_ble != 0;
        FURI_LOG_D(TAG, "External BLE %s", seos->has_external_ble ? "on" : "off");
    } while(false);

    furi_string_free(header);
    flipper_format_free(file);
}

bool seos_settings_save(Seos* seos) {
    furi_assert(seos);

    FlipperFormat* file = flipper_format_file_alloc(seos->credential->storage);
    uint32_t external_ble = seos->has_external_ble ? 1 : 0;
    bool saved = false;

    do {
        if(!flipper_format_file_open_always(file, SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(file, SETTINGS_HEADER, SETTINGS_VERSION)) break;
        if(!flipper_format_write_uint32(file, KEY_EXTERNAL_BLE, &external_ble, 1)) break;
        saved = true;
    } while(false);

    if(!saved) FURI_LOG_E(TAG, "Could not write settings");

    flipper_format_free(file);
    return saved;
}
