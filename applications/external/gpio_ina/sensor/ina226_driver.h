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

#include <stdint.h>

#include "sensor_driver.h"

#define INA226_DRIVER_ID "INA226"

typedef enum {
    Ina226Averaging_1 = 0,
    Ina226Averaging_4 = 1,
    Ina226Averaging_16 = 2,
    Ina226Averaging_64 = 3,
    Ina226Averaging_128 = 4,
    Ina226Averaging_256 = 5,
    Ina226Averaging_512 = 6,
    Ina226Averaging_1024 = 7,
    Ina226Averaging_count,
} Ina226Averaging;

typedef enum {
    Ina226ConvTime_140us = 0,
    Ina226ConvTime_204us = 1,
    Ina226ConvTime_332us = 2,
    Ina226ConvTime_588us = 3,
    Ina226ConvTime_1100us = 4,
    Ina226ConvTime_2116us = 5,
    Ina226ConvTime_4156us = 6,
    Ina226ConvTime_8244us = 7,
    Ina226ConvTime_count,
} Ina226ConvTime;

typedef struct {
    // I2C address of the sensor
    uint8_t i2c_address;
    // Shunt resistor [ohm]
    double shunt_resistor;
    // Averaging mode
    Ina226Averaging averaging;
    // VBUS conversion time
    Ina226ConvTime vbus_conv_time;
    // VSHUNT conversion time
    Ina226ConvTime vshunt_conv_time;
} Ina226Config;

SensorDriver* ina226_driver_alloc(const Ina226Config* config);
