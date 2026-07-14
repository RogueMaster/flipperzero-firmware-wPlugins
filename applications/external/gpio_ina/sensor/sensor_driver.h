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
#include <stdbool.h>

typedef struct {
    // Short string identifying sensor type
    const char* sensor_type;
    // Resistance of the shunt resistor [ohm]
    double shunt_resistor;

    // Sensor detected and ready
    bool ready;

    // Unix timestamp
    uint32_t timestamp;
    // Time of the last measurement [system ticks]
    uint32_t ticks;
    // Current [A]
    double current;
    // BUS voltage [V]
    double voltage;
} SensorState;

typedef struct SensorDriver SensorDriver;

struct SensorDriver {
    void (*free)(SensorDriver* driver);
    bool (*tick)(SensorDriver* driver);
    void (*get_state)(SensorDriver* driver, SensorState* state);
};
