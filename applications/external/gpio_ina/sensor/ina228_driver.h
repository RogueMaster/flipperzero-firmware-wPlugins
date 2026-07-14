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

#define INA228_DRIVER_ID "INA228"

typedef enum {
    Ina228Averaging_1 = 0,
    Ina228Averaging_4 = 1,
    Ina228Averaging_16 = 2,
    Ina228Averaging_64 = 3,
    Ina228Averaging_128 = 4,
    Ina228Averaging_256 = 5,
    Ina228Averaging_512 = 6,
    Ina228Averaging_1024 = 7,
} Ina228Averaging;

typedef enum {
    Ina228ConvTime_50us = 0,
    Ina228ConvTime_84us = 1,
    Ina228ConvTime_150us = 2,
    Ina228ConvTime_280us = 3,
    Ina228ConvTime_540us = 4,
    Ina228ConvTime_1052us = 5,
    Ina228ConvTime_2074us = 6,
    Ina228ConvTime_4120us = 7,
} Ina228ConvTime;

typedef enum {
    Ina228AdcRange_160mV = 0,
    Ina228AdcRange_40mV = 1,
} Ina228AdcRange;

typedef struct {
    // I2C address of the sensor
    uint8_t i2c_address;
    // Shunt resistor [ohm]
    double shunt_resistor;
    // Averaging mode
    Ina228Averaging averaging;
    // VBUS conversion time
    Ina228ConvTime vbus_conv_time;
    // VSHUNT conversion time
    Ina228ConvTime vshunt_conv_time;
    // VSHUNT adc range
    Ina228AdcRange adc_range;
} Ina228Config;

SensorDriver* ina228_driver_alloc(const Ina228Config* config);
