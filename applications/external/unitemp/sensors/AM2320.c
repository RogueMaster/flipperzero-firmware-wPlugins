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

#include "AM2320.h"
#include "interfaces/singlewire_sensor.h"
#include "../interfaces/i2c_sensor.h"

const SensorModel AM2320_SW = {
    .modelname = "AM2320",
    .altname = "AM2320 (single wire)",
    .interface = &unitemp_singlewire,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 2000,
    .allocator = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .initializer = unitemp_singlewire_init,
    .deinitializer = unitemp_singlewire_deinit,
    .updater = unitemp_singlewire_update};

const SensorModel AM2320_I2C = {
    .modelname = "AM2320_I2C",
    .altname = "AM2320 (I2C)",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 2000,
    .allocator = unitemp_AM2320_I2C_alloc,
    .mem_releaser = unitemp_AM2320_I2C_free,
    .initializer = unitemp_AM2320_init,
    .deinitializer = unitemp_AM2320_I2C_deinit,
    .updater = unitemp_AM2320_I2C_update};

static uint16_t AM2320_calc_CRC(uint8_t* ptr, uint8_t len) {
    uint16_t crc = 0xFFFF;
    uint8_t i;
    while(len--) {
        crc ^= *ptr++;
        for(i = 0; i < 8; i++) {
            if(crc & 0x01) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool unitemp_AM2320_I2C_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Addresses on the I2C bus (7 bits)
    i2c_sensor->min_i2c_adress = 0x5C << 1;
    i2c_sensor->max_i2c_adress = 0x5C << 1;
    return true;
}

bool unitemp_AM2320_I2C_free(Sensor* sensor) {
    //Nothing to release since nothing was allocated
    UNUSED(sensor);
    return true;
}

bool unitemp_AM2320_init(Sensor* sensor) {
    //Nothing to initialize
    UNUSED(sensor);
    return true;
}

bool unitemp_AM2320_I2C_deinit(Sensor* sensor) {
    //Nothing to deinitialize
    UNUSED(sensor);
    return true;
}

SensorStatus unitemp_AM2320_I2C_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    uint8_t data[8] = {0x03, 0x00, 0x04};

    //Wake up
    unitemp_i2c_is_device_ready(i2c_sensor);
    furi_delay_ms(1);

    //Request
    if(!unitemp_i2c_write_array(i2c_sensor, 3, data)) return UT_SENSORSTATUS_TIMEOUT;
    furi_delay_ms(2);
    //Answer
    if(!unitemp_i2c_read_array(i2c_sensor, 8, data)) return UT_SENSORSTATUS_TIMEOUT;

    if(AM2320_calc_CRC(data, 6) != ((data[7] << 8) | data[6])) {
        return UT_SENSORSTATUS_BADCRC;
    }

    sensor->humidity = (float)(((uint16_t)data[2] << 8) | data[3]) / 10;
    //Checking for negative temperature
    if(!(data[4] & (1 << 7))) {
        sensor->temperature = (float)(((uint16_t)data[4] << 8) | data[5]) / 10;
    } else {
        data[4] &= ~(1 << 7);
        sensor->temperature = (float)(((uint16_t)data[4] << 8) | data[5]) / -10;
    }
    return UT_SENSORSTATUS_OK;
}
