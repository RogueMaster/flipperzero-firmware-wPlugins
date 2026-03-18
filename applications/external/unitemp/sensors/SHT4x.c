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
#include "SHT4x.h"
#include "../interfaces/i2c_sensor.h"

#define COMMAND_SOFT_RESET                   0x94
#define COMMAND_READ_SN                      0x89
#define COMMAND_MEASURE_WITH_HIGHT_PRECISION 0xFD

bool SHT4x_soft_reset(Sensor* sensor);
uint32_t SHT4x_read_serial_number(Sensor* sensor);
bool SHT4x_read_th(Sensor* sensor);

const SensorModel SHT4x = {
    .modelname = "SHT4x",
    .altname = "SHT40/41/43/45",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 1000,
    .allocator = unitemp_SHT4x_alloc,
    .mem_releaser = unitemp_SHT4x_free,
    .initializer = unitemp_SHT4x_init,
    .deinitializer = unitemp_SHT4x_deinit,
    .updater = unitemp_SHT4x_update};

bool unitemp_SHT4x_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Addresses on the I2C bus (7 bits)
    i2c_sensor->min_i2c_adress = 0x44 << 1;
    i2c_sensor->max_i2c_adress = 0x46 << 1;
    return true;
}

bool unitemp_SHT4x_free(Sensor* sensor) {
    //Nothing to release since nothing was allocated
    UNUSED(sensor);
    return true;
}

bool unitemp_SHT4x_init(Sensor* sensor) {
    if(!SHT4x_soft_reset(sensor)) return false;
    furi_delay_ms(2);
    uint32_t sn = SHT4x_read_serial_number(sensor);
    if(!sn) return false;
    UNITEMP_DEBUG("%s serial number: %04lX", sensor->name, sn);

    return true;
}

bool unitemp_SHT4x_deinit(Sensor* sensor) {
    //Nothing to deinitialize
    UNUSED(sensor);
    return true;
}

SensorStatus unitemp_SHT4x_update(Sensor* sensor) {
    if(!SHT4x_read_th(sensor)) return UT_SENSORSTATUS_TIMEOUT;
    return UT_SENSORSTATUS_OK;
}

uint8_t SHT4x_crc8(const uint8_t* data, const size_t len) {
    // From Sensirion application note: polynomial 0x31, init 0xFF
    uint8_t crc = 0xFF;

    for(size_t byte = 0; byte < len; ++byte) {
        crc ^= data[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }

    return crc;
}

bool SHT4x_soft_reset(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    uint8_t buff[1] = {COMMAND_SOFT_RESET};
    return unitemp_i2c_write_array(i2c_sensor, 1, buff);
}

uint32_t SHT4x_read_serial_number(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    uint8_t buff[6] = {COMMAND_READ_SN};
    if(!unitemp_i2c_write_array(i2c_sensor, 1, buff)) return 0;
    furi_delay_ms(1);
    if(!unitemp_i2c_read_array(i2c_sensor, 6, buff)) return 0;
    if(SHT4x_crc8(buff, 2) != buff[2]) return 0;
    if(SHT4x_crc8(buff + 3, 2) != buff[5]) return 0;

    return ((uint32_t)buff[0] << 24) | ((uint32_t)buff[1] << 16) | ((uint32_t)buff[3] << 8) |
           buff[4];
}

bool SHT4x_read_th(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    uint8_t buff[6] = {COMMAND_MEASURE_WITH_HIGHT_PRECISION};
    if(!unitemp_i2c_write_array(i2c_sensor, 1, buff)) return false;
    furi_delay_ms(9);

    if(!unitemp_i2c_read_array(i2c_sensor, 6, buff)) return false;

    if(SHT4x_crc8(buff, 2) != buff[2]) return false;
    if(SHT4x_crc8(buff + 3, 2) != buff[5]) return false;

    uint16_t t = (buff[0] << 8) | buff[1];
    sensor->temperature = -45.0f + 175.0f * t / 65535.0f;
    uint16_t h = (buff[3] << 8) | buff[4];
    sensor->humidity = -6.0f + 125.0f * h / 65535.0f;

    if(sensor->humidity > 100) sensor->humidity = 100.0f;
    if(sensor->humidity < 0) sensor->humidity = 0.0f;

    return true;
}
