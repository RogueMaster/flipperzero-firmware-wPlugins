// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_settings_storage.h"

#include <flipper_format/flipper_format.h>
#include <stddef.h>

#define SETTINGS_FILETYPE  "Taiko Player Settings"
#define SETTINGS_VERSION   1U
#define SETTINGS_TEMP_PATH TAIKO_SETTINGS_PATH ".tmp"

typedef struct {
    const char* key;
    size_t offset;
} SettingsField;

static const SettingsField fields[] = {
    {"MasterVolume", offsetof(TaikoSettings, volume)},
    {"DongVolume", offsetof(TaikoSettings, don_volume)},
    {"KaVolume", offsetof(TaikoSettings, ka_volume)},
    {"BackwardInterval", offsetof(TaikoSettings, backward_ms)},
    {"ForwardInterval", offsetof(TaikoSettings, forward_ms)},
    {"SeekMode", offsetof(TaikoSettings, seek_mode)},
    {"BackwardBars", offsetof(TaikoSettings, backward_bars)},
    {"ForwardBars", offsetof(TaikoSettings, forward_bars)},
    {"PlaybackSpeed", offsetof(TaikoSettings, playback_tenths)},
    {"ScrollSpeed", offsetof(TaikoSettings, scroll_tenths)},
    {"FrameRate", offsetof(TaikoSettings, frame_rate)},
    {"GpioPulseMs", offsetof(TaikoSettings, gpio_pulse_ms)}};
static const char* pin_keys[] = {"LeftKaPin", "LeftDongPin", "RightDongPin", "RightKaPin"};

bool taiko_settings_load(Storage* storage, TaikoSettings* settings) {
    taiko_settings_defaults(settings);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* text = furi_string_alloc();
    uint32_t version = 0;
    bool opened = flipper_format_file_open_existing(file, TAIKO_SETTINGS_PATH);
    bool header = opened && flipper_format_read_header(file, text, &version) &&
                  furi_string_equal_str(text, SETTINGS_FILETYPE) && version == SETTINGS_VERSION;
    bool complete = header;
    if(header) {
        for(size_t i = 0; i < COUNT_OF(fields); i++) {
            uint32_t value;
            if(flipper_format_rewind(file) &&
               flipper_format_read_string(file, fields[i].key, text) &&
               taiko_settings_parse_uint(furi_string_get_cstr(text), &value)) {
                *(uint32_t*)((uint8_t*)settings + fields[i].offset) = value;
            } else {
                complete = false;
            }
        }
        for(size_t i = 0; i < COUNT_OF(pin_keys); i++) {
            if(!flipper_format_rewind(file) ||
               !flipper_format_read_string(file, pin_keys[i], text) ||
               !taiko_settings_parse_pin(furi_string_get_cstr(text), &settings->pins[i]))
                complete = false;
        }
        if(!taiko_settings_validate(settings)) complete = false;
    }
    if(opened) flipper_format_file_close(file);
    furi_string_free(text);
    flipper_format_free(file);
    return complete;
}

bool taiko_settings_save(Storage* storage, const TaikoSettings* settings) {
    TaikoSettings valid = *settings;
    if(!taiko_settings_validate(&valid)) return false;
    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool opened = flipper_format_file_open_always(file, SETTINGS_TEMP_PATH);
    bool success = opened &&
                   flipper_format_write_header_cstr(file, SETTINGS_FILETYPE, SETTINGS_VERSION);
    for(size_t i = 0; i < COUNT_OF(fields) && success; i++) {
        const uint32_t* value = (const uint32_t*)((const uint8_t*)settings + fields[i].offset);
        success = flipper_format_write_uint32(file, fields[i].key, value, 1);
    }
    for(size_t i = 0; i < COUNT_OF(pin_keys) && success; i++) {
        success = flipper_format_write_string_cstr(
            file, pin_keys[i], taiko_gpio_pin_name(settings->pins[i]));
    }
    if(opened && !flipper_format_file_close(file)) success = false;
    flipper_format_free(file);
    if(success)
        success = storage_common_rename(storage, SETTINGS_TEMP_PATH, TAIKO_SETTINGS_PATH) ==
                  FSE_OK;
    if(!success && opened) storage_common_remove(storage, SETTINGS_TEMP_PATH);
    return success;
}
