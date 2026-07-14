/*
    Unitemp - Universal temperature reader
    Copyright (C) 2023  Victor Nikitchuk (https://github.com/quen0n)

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
#include "HTU21x.h"
#include "../interfaces/i2c_sensor.h"

const SensorModel HTU21x = {
    .modelname = "HTU21x",
    .altname = "HTU21D(F)",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 250,
    .allocator = unitemp_HTU21x_alloc,
    .mem_releaser = unitemp_HTU21x_free,
    .initializer = unitemp_HTU21x_init,
    .deinitializer = unitemp_HTU21x_deinit,
    .updater = unitemp_HTU21x_update};
const SensorModel SI7021 = {
    .modelname = "SI7021",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 250,
    .allocator = unitemp_HTU21x_alloc,
    .mem_releaser = unitemp_HTU21x_free,
    .initializer = unitemp_HTU21x_init,
    .deinitializer = unitemp_HTU21x_deinit,
    .updater = unitemp_HTU21x_update};
const SensorModel SHT2x = {
    .modelname = "SHT2x",
    .altname = "SHT20/21/25",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 250,
    .allocator = unitemp_HTU21x_alloc,
    .mem_releaser = unitemp_HTU21x_free,
    .initializer = unitemp_HTU21x_init,
    .deinitializer = unitemp_HTU21x_deinit,
    .updater = unitemp_HTU21x_update};

static uint8_t checkCRC(uint16_t data) {
    for(uint8_t i = 0; i < 16; i++) {
        if(data & 0x8000)
            data = (data << 1) ^ 0x13100;
        else
            data <<= 1;
    }
    return (data >> 8);
}

bool unitemp_HTU21x_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Addresses on the I2C bus (7 bits)
    i2c_sensor->min_i2c_adress = 0x40 << 1;
    i2c_sensor->max_i2c_adress = 0x41 << 1;
    return true;
}

bool unitemp_HTU21x_free(Sensor* sensor) {
    //Nothing to release since nothing was allocated
    UNUSED(sensor);
    return true;
}

bool unitemp_HTU21x_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    UNUSED(i2c_sensor);
    return true;
}

bool unitemp_HTU21x_deinit(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    UNUSED(i2c_sensor);
    return true;
}

SensorStatus unitemp_HTU21x_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //There can be only one sensor, so it’s normal
    static bool temp_hum = true;
    static bool req_read = true;

    uint8_t data[3];

    if(req_read) {
        if(temp_hum) {
            //Request temperature
            data[0] = 0xF3;
            if(!unitemp_i2c_write_array(i2c_sensor, 1, data)) return UT_SENSORSTATUS_TIMEOUT;
        } else {
            //Humidity request
            data[0] = 0xF5;
            if(!unitemp_i2c_write_array(i2c_sensor, 1, data)) return UT_SENSORSTATUS_TIMEOUT;
        }
        req_read = !req_read;
    } else {
        req_read = !req_read;
        if(!unitemp_i2c_read_array(i2c_sensor, 3, data)) return UT_SENSORSTATUS_TIMEOUT;

        uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
        if(checkCRC(raw) != data[2]) return UT_SENSORSTATUS_BADCRC;

        if(temp_hum) {
            sensor->temperature = (0.002681f * raw - 46.85f);
        } else {
            sensor->humidity = ((0.001907 * (raw ^ 0x02)) - 6);
        }
        temp_hum = !temp_hum;
    }
    if(sensor->temperature == -128.0f || sensor->humidity == -128.0f) {
        return UT_SENSORSTATUS_POLLING;
    }

    return UT_SENSORSTATUS_OK;
}
