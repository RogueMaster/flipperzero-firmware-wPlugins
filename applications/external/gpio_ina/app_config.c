/*
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
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

#include "app_common.h"
#include "app_config.h"
#include "slice.h"

#define CONFIG_FILE_NAME APP_DATA_PATH("config.ini")

#define SECTION_APP    "app"
#define SECTION_SENSOR "sensor"

#define KEY_SENSOR_TYPE        "sensorType"
#define KEY_I2C_ADDRESS        "i2cAddress"
#define KEY_SHUNT_RESISTOR     "shuntResistor"
#define KEY_SHUNT_RESISTOR_ALT "altShuntResistor"
#define KEY_VOLTAGE_PRECISION  "voltagePrecision"
#define KEY_CURRENT_PRECISION  "currentPrecision"
#define KEY_SENSOR_AVERAGING   "sensorAveraging"
#define KEY_LED_BLINKING       "ledBlinking"

void app_config_init(AppConfig* config) {
    furi_check(config != NULL);
    config->sensor_type = SensorType_INA219;
    config->i2c_address = I2C_ADDRESS_MIN;
    config->shunt_resistor = 100000; // 100mOhm
    config->shunt_resistor_alt = 100000; // 100mOhm
    config->voltage_precision = SensorPrecision_Medium;
    config->current_precision = SensorPrecision_Medium;
    config->sensor_averaging = SensorAveraging_Max;
    config->led_blinking = true;
}

const char* sensor_type_name(SensorType sensor_type) {
    switch(sensor_type) {
    case SensorType_INA219:
        return "INA219";
    case SensorType_INA226:
        return "INA226";
    case SensorType_INA228:
        return "INA228";
    default:
        return "Unknown";
    }
}

const char* sensor_precision_name(SensorPrecision sensor_mode) {
    switch(sensor_mode) {
    case SensorPrecision_Low:
        return "Low";
    case SensorPrecision_Medium:
        return "Medium";
    case SensorPrecision_High:
        return "High";
    case SensorPrecision_Max:
        return "Max";
    default:
        return "Unknown";
    }
}

const char* sensor_averaging_name(SensorAveraging sensor_averaging) {
    switch(sensor_averaging) {
    case SensorAveraging_Medium:
        return "Medium";
    case SensorAveraging_Max:
        return "Max";
    default:
        return "Unknown";
    }
}

#define ini_add_comment(s, comment)      furi_string_cat_printf(s, "# %s\n", comment)
#define ini_add_section(s, section)      furi_string_cat_printf(s, "[%s]\n", section)
#define ini_add_keyval(s, key, fmt, val) furi_string_cat_printf(s, "%s = " fmt "\n", key, val)
#define ini_add_empty_line(s)            furi_string_cat_printf(s, "\n")

static FuriString* app_config_build(const AppConfig* config) {
    FuriString* s = furi_string_alloc();

    ini_add_section(s, SECTION_APP);
    ini_add_keyval(s, KEY_LED_BLINKING, "%s", config->led_blinking ? "1" : "0");
    ini_add_empty_line(s);

    ini_add_section(s, SECTION_SENSOR);
    ini_add_keyval(s, KEY_SENSOR_TYPE, "%s", sensor_type_name(config->sensor_type));
    ini_add_keyval(s, KEY_I2C_ADDRESS, "0x%02X", config->i2c_address);
    ini_add_keyval(s, KEY_SHUNT_RESISTOR, "%f", config->shunt_resistor);
    ini_add_keyval(s, KEY_SHUNT_RESISTOR_ALT, "%f", config->shunt_resistor_alt);
    ini_add_keyval(
        s, KEY_VOLTAGE_PRECISION, "%s", sensor_precision_name(config->voltage_precision));
    ini_add_keyval(
        s, KEY_CURRENT_PRECISION, "%s", sensor_precision_name(config->current_precision));
    ini_add_keyval(s, KEY_SENSOR_AVERAGING, "%s", sensor_averaging_name(config->sensor_averaging));

    return s;
}

#define section(str) slice_equals_cstr(section, str)
#define key(str)     slice_equals_cstr(key, str)

static void app_config_set(AppConfig* config, Slice section, Slice key, Slice value) {
    if(section(SECTION_APP)) {
        if(key(KEY_LED_BLINKING)) {
            uint32_t temp;
            if(slice_parse_uint32(value, 0, &temp)) {
                config->led_blinking = temp != 0;
                FURI_LOG_I(TAG, "led_blinking=%d", config->led_blinking);
            } else {
                FURI_LOG_E(TAG, "Failed to parse led_blinking value");
            }
        } else {
            FURI_LOG_E(TAG, "Unknown key");
        }
    } else if(section(SECTION_SENSOR)) {
        if(key(KEY_SENSOR_TYPE)) {
            bool found = false;
            for(SensorType i = 0; i < SensorType_count; i++) {
                if(slice_equals_cstr(value, sensor_type_name(i))) {
                    config->sensor_type = i;
                    found = true;
                    break;
                }
            }
            if(found) {
                FURI_LOG_I(TAG, "sensor_type=%s", sensor_type_name(config->sensor_type));
            } else {
                FURI_LOG_E(TAG, "Unknown sensor type");
            }
        } else if(key(KEY_I2C_ADDRESS)) {
            uint32_t temp;
            if(slice_parse_uint32(value, 0, &temp) && temp <= 127) {
                config->i2c_address = (uint8_t)temp;
                FURI_LOG_I(TAG, "i2c_address=0x%02X", config->i2c_address);
            } else {
                FURI_LOG_E(TAG, "Failed to parse I2C address value");
            }
        } else if(key(KEY_SHUNT_RESISTOR)) {
            double temp;
            if(slice_parse_double(value, &temp)) {
                config->shunt_resistor = temp;
                FURI_LOG_I(
                    TAG, "shunt_resistore=%.3fmOhm", config->shunt_resistor * (double)1000.0);
            } else {
                FURI_LOG_E(TAG, "Failed to parse shunt resistor value");
            }
        } else if(key(KEY_SHUNT_RESISTOR_ALT)) {
            double temp;
            if(slice_parse_double(value, &temp)) {
                config->shunt_resistor_alt = temp;
                FURI_LOG_I(
                    TAG,
                    "shunt_resistor_alt=%.3fmOhm",
                    config->shunt_resistor_alt * (double)1000.0);
            } else {
                FURI_LOG_E(TAG, "Failed to parse alternate shunt resistor value");
            }
        } else if(key(KEY_VOLTAGE_PRECISION)) {
            bool found = false;
            for(SensorPrecision i = 0; i < SensorPrecision_count; i++) {
                if(slice_equals_cstr(value, sensor_precision_name(i))) {
                    config->voltage_precision = i;
                    found = true;
                    break;
                }
            }
            if(found) {
                FURI_LOG_I(
                    TAG, "voltage_precision=%s", sensor_precision_name(config->voltage_precision));
            } else {
                FURI_LOG_E(TAG, "Unknown voltage precision");
            }
        } else if(key(KEY_CURRENT_PRECISION)) {
            bool found = false;
            for(SensorPrecision i = 0; i < SensorPrecision_count; i++) {
                if(slice_equals_cstr(value, sensor_precision_name(i))) {
                    config->current_precision = i;
                    found = true;
                    break;
                }
            }
            if(found) {
                FURI_LOG_I(
                    TAG, "current_precision=%s", sensor_precision_name(config->current_precision));
            } else {
                FURI_LOG_E(TAG, "Unknown current precision");
            }
        } else if(key(KEY_SENSOR_AVERAGING)) {
            bool found = false;
            for(SensorAveraging i = 0; i < SensorAveraging_count; i++) {
                if(slice_equals_cstr(value, sensor_averaging_name(i))) {
                    config->sensor_averaging = i;
                    found = true;
                    break;
                }
            }
            if(found) {
                FURI_LOG_I(
                    TAG, "sensor_averaging=%s", sensor_averaging_name(config->sensor_averaging));
            } else {
                FURI_LOG_E(TAG, "Unknown sensor averaging");
            }
        }

        else {
            FURI_LOG_E(TAG, "Unknown key");
        }
    }
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
            app_config_set(config, section, key, value);
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
            FURI_LOG_E(TAG, "Failed to write file");
        }
        furi_string_free(config_string);
        FURI_LOG_I(TAG, "Configuration stored");
    } else {
        FURI_LOG_E(TAG, "Failed to open file");
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
                FURI_LOG_E(TAG, "Failed to read config file");
            }

            free(buff);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to open config file");
    }

    storage_file_close(file);
    storage_file_free(file);
}
