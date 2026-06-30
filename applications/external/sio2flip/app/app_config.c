/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <storage/storage.h>

#include "utils/slice.h"

#include "app_common.h"
#include "app_config.h"

#define CONFIG_FILE_NAME APP_DATA_PATH("config.ini")

#define SECTION_APP "app"

#define KEY_LED_BLINKING "ledBlinking"
#define KEY_SPEED_MODE   "speedMode"
#define KEY_SPEED_INDEX  "speedIndex"
#define KEY_ATARI850     "atari850"

#define SECTION_FDD "fdd."

#define KEY_DISK_IMAGE "diskImage"

void app_config_init(AppConfig* config) {
    furi_check(config != NULL);
    memset(config, 0, sizeof(AppConfig));

    config->led_blinking = true;
    config->speed_mode = SpeedMode_Standard;
    config->speed_index = SpeedIndex_8_56K;
    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        config->fdd[i].image = furi_string_alloc();
    }
}

void app_config_free(AppConfig* config) {
    if(config != NULL) {
        for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
            if(config->fdd[i].image != NULL) {
                furi_string_free(config->fdd[i].image);
            }
        }
    }
}

const AppConfigOption speed_mode_options[] = {
    {SpeedMode_Standard, "standard", "Standard"},
    {SpeedMode_USDoubler, "us-doubler", "US Dblr"},
    {SpeedMode_XF551, "xf-551", "XF551"},
};

const AppConfigOption* speed_mode_by_value(SpeedMode mode) {
    for(size_t i = 0; i < ARRAY_SIZE(speed_mode_options); i++) {
        if(speed_mode_options[i].value == mode) {
            return &speed_mode_options[i];
        }
    }
    return NULL;
}

const AppConfigOption* speed_mode_by_id(Slice slice) {
    for(size_t i = 0; i < ARRAY_SIZE(speed_mode_options); i++) {
        if(slice_equals_cstr(slice, speed_mode_options[i].id)) {
            return &speed_mode_options[i];
        }
    }
    return NULL;
}

const AppConfigOption speed_index_options[] = {
    {SpeedIndex_16_38K, "16", "38K"},
    {SpeedIndex_8_56K, "8", "57K"},
    {SpeedIndex_7_61K, "7", "61K"},
    {SpeedIndex_6_68K, "6", "68K"},
    {SpeedIndex_5_74K, "5", "74K"},
    {SpeedIndex_4_81K, "4", "81K"},
    {SpeedIndex_3_89K, "3", "89K"},
    {SpeedIndex_2_99K, "2", "99K"},
    {SpeedIndex_1_111K, "1", "111K"},
    {SpeedIndex_0_127K, "0", "127K"}};

const AppConfigOption* speed_index_by_value(SpeedIndex value) {
    for(size_t i = 0; i < ARRAY_SIZE(speed_index_options); i++) {
        if(speed_index_options[i].value == value) {
            return &speed_index_options[i];
        }
    }
    return NULL;
}

const AppConfigOption* speed_index_by_id(Slice slice) {
    for(size_t i = 0; i < ARRAY_SIZE(speed_index_options); i++) {
        if(slice_equals_cstr(slice, speed_index_options[i].id)) {
            return &speed_index_options[i];
        }
    }
    return NULL;
}

static bool slice_to_bool(Slice slice, bool* value) {
    if(slice_equals_cstr(slice, "1")) {
        *value = true;
        return true;
    } else if(slice_equals_cstr(slice, "0")) {
        *value = false;
        return true;
    }
    return false;
}

static bool slice_to_fdd_index(Slice text, uint32_t* index) {
    return slice_parse_uint32(text, 0, index) && *index < FDD_EMULATOR_COUNT;
}

static bool slice_to_image_path(Slice slice, FuriString* path) {
    furi_string_set_strn(path, slice.start, slice_len(slice));
    return true;
}

#define ini_add_comment(s, comment)      furi_string_cat_printf(s, "# %s\n", comment)
#define ini_add_section(s, section)      furi_string_cat_printf(s, "[%s]\n", section)
#define ini_add_keyval(s, key, fmt, val) furi_string_cat_printf(s, "%s = " fmt "\n", key, val)
#define ini_add_empty_line(s)            furi_string_cat_printf(s, "\n")

#define ini_add_fdd_section(s, i) furi_string_cat_printf(s, "[%s%d]\n", SECTION_FDD, i)

static FuriString* app_config_build(const AppConfig* config) {
    FuriString* s = furi_string_alloc();

    ini_add_section(s, SECTION_APP);
    ini_add_keyval(s, KEY_LED_BLINKING, "%s", config->led_blinking ? "1" : "0");
    ini_add_keyval(s, KEY_SPEED_MODE, "%s", speed_mode_by_value(config->speed_mode)->id);
    ini_add_keyval(s, KEY_SPEED_INDEX, "%s", speed_index_by_value(config->speed_index)->id);
    ini_add_keyval(s, KEY_ATARI850, "%s", config->atari850 ? "1" : "0");
    ini_add_empty_line(s);

    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        ini_add_fdd_section(s, i);
        ini_add_keyval(s, KEY_DISK_IMAGE, "%s", furi_string_get_cstr(config->fdd[i].image));
        ini_add_empty_line(s);
    }

    return s;
}

#define section(str) slice_equals_cstr(section, str)
#define key(str)     slice_equals_cstr(key, str)

#define section_starts_with(str) slice_starts_with_cstr(section, str)

static bool app_config_set(AppConfig* config, Slice section, Slice key, Slice value) {
    if(section(SECTION_APP)) {
        if(key(KEY_LED_BLINKING)) {
            return slice_to_bool(value, &config->led_blinking);
        } else if(key(KEY_SPEED_MODE)) {
            const AppConfigOption* option = speed_mode_by_id(value);
            if(option != NULL) {
                config->speed_mode = option->value;
                return true;
            }
        } else if(key(KEY_SPEED_INDEX)) {
            const AppConfigOption* option = speed_index_by_id(value);
            if(option != NULL) {
                config->speed_index = option->value;
                return true;
            }
        } else if(key(KEY_ATARI850)) {
            return slice_to_bool(value, &config->atari850);
        }
    } else if(section_starts_with(SECTION_FDD)) {
        section.start += strlen(SECTION_FDD);
        uint32_t fdd_idx;
        if(!slice_to_fdd_index(section, &fdd_idx)) {
            return false;
        }

        if(key(KEY_DISK_IMAGE)) {
            return slice_to_image_path(value, config->fdd[fdd_idx].image);
        }
    }
    return false;
}

static void app_config_parse(AppConfig* config, Slice text) {
    Slice section = slice_empty();
    while(!slice_is_empty(text)) {
        Slice line = slice_trim_left(tok_until_eol(&text));
        if(slice_is_empty(line) || tok_skip_char(&line, '#')) {
            // Skip line
        } else if(tok_skip_char(&line, '[')) {
            // Change section
            section = slice_trim(tok_until_char(&line, ']'));
        } else {
            // Parse key-value pair
            Slice key = slice_trim(tok_until_char(&line, '='));
            Slice value = slice_empty();
            if(tok_skip_char(&line, '=')) {
                value = slice_trim(line);
            }
            if(!app_config_set(config, section, key, value)) {
                FURI_LOG_E(
                    TAG,
                    "Failed to parse %.*s.%.*s = %.*s",
                    slice_len(section),
                    slice_start(section),
                    slice_len(key),
                    slice_start(key),
                    slice_len(value),
                    slice_start(value));
            }
        }
        tok_skip_eol(&text);
    }
}

void app_config_save(const AppConfig* config, Storage* storage) {
    furi_check(config != NULL);
    furi_check(storage != NULL);

    FURI_LOG_I(TAG, "Storing configuration...");

    File* file = storage_file_alloc(storage);
    furi_check(file != NULL, "Failed to allocate file");

    if(storage_file_open(file, CONFIG_FILE_NAME, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* config_string = app_config_build(config);
        const char* cstr = furi_string_get_cstr(config_string);
        size_t cstr_len = furi_string_size(config_string);
        if(storage_file_write(file, cstr, cstr_len) != cstr_len) {
            FURI_LOG_E(TAG, "Failed to write file %s", CONFIG_FILE_NAME);
        }
        furi_string_free(config_string);
        FURI_LOG_I(TAG, "Configuration stored to %s", CONFIG_FILE_NAME);
    } else {
        FURI_LOG_E(TAG, "Failed to open file %s", CONFIG_FILE_NAME);
    }

    storage_file_close(file);
    storage_file_free(file);
}

void app_config_load(AppConfig* config, Storage* storage) {
    furi_check(config != NULL);
    furi_check(storage != NULL);

    FURI_LOG_I(TAG, "Loading configuration...");

    File* file = storage_file_alloc(storage);
    furi_check(file != NULL, "Failed to allocate file");

    if(storage_file_open(file, CONFIG_FILE_NAME, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t buff_size = storage_file_size(file);
        if(buff_size > 0) {
            char* buff = malloc(buff_size);

            if(storage_file_read(file, buff, buff_size) == buff_size) {
                Slice text = {buff, buff + buff_size};
                FURI_LOG_I(TAG, "Parsing configuration...");
                app_config_parse(config, text);
            } else {
                FURI_LOG_E(TAG, "Failed to read config file %s", CONFIG_FILE_NAME);
            }

            free(buff);
        } else {
            FURI_LOG_E(TAG, "Failed to open config file %s", CONFIG_FILE_NAME);
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    FURI_LOG_I(TAG, "Configuration loaded");
}
