/*
    Unitemp - Universal temperature reader
    Copyright (C) 2025  Jakub Kakona (https://github.com/kaklik)
    
    MAX31725 Temperature Sensor Driver for Flipper Zero
 
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "MAX31725.h"
#include "../interfaces/I2CSensor.h"

// I2C registers
#define MAX31725_REG_TEMP       0x00
#define MAX31725_REG_CONFIG     0x01

// Configuration bits (datasheet Table 5)

#define MAX31725_CONFIG_ONE_SHOT         (1 << 7)
#define MAX31725_CONFIG_TIMEOUT_DISABLE  (1 << 6)
#define MAX31725_CONFIG_TIMEOUT_ENABLE   (0 << 6)
#define MAX31725_CONFIG_EXTENDED_FORMAT  (1 << 5)
#define MAX31725_CONFIG_NORMAL_FORMAT    (0 << 5)
#define MAX31725_CONFIG_FAULTQUEUE_1     (0 << 3)
#define MAX31725_CONFIG_FAULTQUEUE_2     (1 << 3)
#define MAX31725_CONFIG_FAULTQUEUE_4     (2 << 3)
#define MAX31725_CONFIG_FAULTQUEUE_6     (3 << 3)
#define MAX31725_CONFIG_OS_POL_HIGH      (1 << 2)
#define MAX31725_CONFIG_OS_POL_LOW       (0 << 2)
#define MAX31725_CONFIG_COMPARATOR_MODE  (0 << 1)
#define MAX31725_CONFIG_INTERRUPT_MODE   (1 << 1)
#define MAX31725_CONFIG_SHUTDOWN         (1 << 0)

const SensorType MAX31725 = {
    .typename        = "MAX31725",
    .interface       = &I2C,
    .datatype        = UT_DATA_TYPE_TEMP,
    .pollingInterval = 500,
    .allocator       = unitemp_MAX31725_alloc,
    .mem_releaser    = unitemp_MAX31725_free,
    .initializer     = unitemp_MAX31725_init,
    .deinitializer   = unitemp_MAX31725_deinit,
    .updater         = unitemp_MAX31725_update
};

bool unitemp_MAX31725_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    // I2C address range: 0x48–0x4F
    i2c_sensor->minI2CAdr = (0x48 << 1);
    i2c_sensor->maxI2CAdr = (0x4F << 1);
    return true;
}

bool unitemp_MAX31725_free(Sensor* sensor) {
    UNUSED(sensor);
    return true;
}

bool unitemp_MAX31725_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    // Default configuration: comparator mode, OS low, 1-fault queue,
    // normal format, timeout enabled
    uint8_t cfg = MAX31725_CONFIG_COMPARATOR_MODE |
                  MAX31725_CONFIG_OS_POL_HIGH   |
                  MAX31725_CONFIG_FAULTQUEUE_1  |
                  MAX31725_CONFIG_NORMAL_FORMAT |
                  MAX31725_CONFIG_TIMEOUT_ENABLE;
    return unitemp_i2c_writeReg(i2c_sensor, MAX31725_REG_CONFIG, cfg);
}

bool unitemp_MAX31725_deinit(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    // sensor shutdown to reduce energy consumtion
    uint8_t cfg = MAX31725_CONFIG_SHUTDOWN;
    return unitemp_i2c_writeReg(i2c_sensor, MAX31725_REG_CONFIG, cfg);
}

UnitempStatus unitemp_MAX31725_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    uint8_t buff[2];
    if (!unitemp_i2c_readRegArray(i2c_sensor, MAX31725_REG_TEMP, 2, buff))
        return UT_SENSORSTATUS_TIMEOUT;
    int16_t raw = ((int16_t)buff[0] << 8) | buff[1];
    // Q8.8 format: LSB = 0.00390625°C
    sensor->temp = raw * 0.00390625;
    return UT_SENSORSTATUS_OK;
}

