/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

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
#include "TMP102.h"
#include "../interfaces/i2c_sensor.h"

#define CONVERSION_RATE_0_25_HZ 0b00
#define CONVERSION_RATE_1_HZ    0b01
#define CONVERSION_RATE_4_HZ    0b10
#define CONVERSION_RATE_8_HZ    0b11

#define CONVERSION_RATE CONVERSION_RATE_4_HZ

#define TEMP_REG  0b00
#define CONF_REG  0b01
#define TLOW_REG  0b10
#define THIGH_REG 0b10

const SensorModel TMP102 = {
    .modelname = "TMP102",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP,
    .polling_interval = 250,
    .allocator = unitemp_TMP102_alloc,
    .mem_releaser = unitemp_TMP102_free,
    .initializer = unitemp_TMP102_init,
    .deinitializer = unitemp_TMP102_deinit,
    .updater = unitemp_TMP102_update};

bool unitemp_TMP102_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Addresses on the I2C bus (7 bits)
    i2c_sensor->min_i2c_adress = 0b1001000 << 1;
    i2c_sensor->max_i2c_adress = 0b1001011 << 1;
    return true;
}

bool unitemp_TMP102_free(Sensor* sensor) {
    //Nothing to release since nothing was allocated
    UNUSED(sensor);
    return true;
}

bool unitemp_TMP102_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    uint8_t buff[3] = {CONF_REG, 0b01100000, (CONVERSION_RATE << 6)};

    if(!unitemp_i2c_write_array(i2c_sensor, 3, buff)) return false;

    return true;
}

bool unitemp_TMP102_deinit(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    //go to shutdown mode
    uint8_t buff[3] = {CONF_REG, 0b01100001, (CONVERSION_RATE << 6)};

    if(!unitemp_i2c_write_array(i2c_sensor, 3, buff)) return false;
    return true;
}

SensorStatus unitemp_TMP102_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    uint8_t buff[2] = {TEMP_REG};
    if(!unitemp_i2c_write_array(i2c_sensor, 1, buff)) return UT_SENSORSTATUS_TIMEOUT;
    if(!unitemp_i2c_read_array(i2c_sensor, 2, buff)) return UT_SENSORSTATUS_TIMEOUT;

    uint16_t temp_reg = (((buff[0] << 8) | buff[1]) >> 4) & 0b011111111111;
    bool temp_is_negative = (buff[0] & 0b10000000) ? true : false;

    sensor->temperature = temp_reg * 0.0625f;
    if(temp_is_negative) sensor->temperature *= -1.0f;

    return UT_SENSORSTATUS_OK;
}
