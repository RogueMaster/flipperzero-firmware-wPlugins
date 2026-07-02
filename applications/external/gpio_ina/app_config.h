/*
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
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

#pragma once

#include <storage/storage.h>

// Supported sensor types
typedef enum {
    SensorType_INA219,
    SensorType_INA226,
    SensorType_INA228,
    SensorType_count,
} SensorType;

typedef enum {
    SensorPrecision_Low,
    SensorPrecision_Medium,
    SensorPrecision_High,
    SensorPrecision_Max,
    SensorPrecision_count,
} SensorPrecision;

typedef enum {
    SensorAveraging_Medium,
    SensorAveraging_Max,
    SensorAveraging_count,
} SensorAveraging;

// Returns the name of the sensor type
const char* sensor_type_name(SensorType sensor_type);

// Returns the name of the sensor mode
const char* sensor_precision_name(SensorPrecision sensor_mode);

// Returns the name of the sensor averaging
const char* sensor_averaging_name(SensorAveraging sensor_averaging);

#define I2C_ADDRESS_MIN   0x40
#define I2C_ADDRESS_COUNT 16

// Application configuration
typedef struct {
    // Sensor type
    SensorType sensor_type;
    // Sensor I2C Address
    uint8_t i2c_address;
    // Shunt resistor value in micro ohms
    double shunt_resistor;
    // Alternate shunt resistor value in micro ohms
    double shunt_resistor_alt;
    // VBUS voltage measurement precision
    SensorPrecision voltage_precision;
    // Shunt current measurement precision
    SensorPrecision current_precision;
    // Sensor averaging mode
    SensorAveraging sensor_averaging;
    // Indicate a new measurement by blinking the LED
    bool led_blinking;
} AppConfig;

// Initializes the application configuration and sets the default values
void app_config_init(AppConfig* config);

// Saves the application configuration to the storage
void app_config_save(const AppConfig* config, Storage* storage);

// Loads the application configuration from the storage
void app_config_load(AppConfig* config, Storage* storage);
