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

#define INA209_DRIVER_ID "INA209"

typedef struct {
    // I2C address of the sensor
    uint8_t i2c_address;
    // Shunt resistor in ohms
    double shunt_resistor;
} Ina209Config;

SensorDriver* ina209_driver_alloc(const Ina209Config* config);
