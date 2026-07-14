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

#include "SHTC3.h"
#include "../interfaces/i2c_sensor.h"

#define SHTC3_COMMAND_SLEEP                        0xB098
#define SHTC3_COMMAND_WAKEUP                       0x3517
#define SHTC3_COMMAND_MEASUREMENT_NORMAL_READ_TEMP 0x7866
#define SHTC3_COMMAND_MEASUREMENT_NORMAL_READ_HUM  0x58E0
#define SHTC3_COMMAND_SOFT_RESET                   0x805D
#define SHTC3_COMMAND_READ_SERIAL_NUMBER           0xEFC8

static bool SHTC3_send_cmd(I2CSensor* sensor, uint16_t cmd);
static bool SHTC3_soft_reset(I2CSensor* sensor);
static bool SHTC3_wakeup(I2CSensor* sensor);
static bool SHTC3_sleep(I2CSensor* sensor);

static uint16_t SHTC3_read_serial_number(I2CSensor* sensor);
static uint8_t unitemp_SHTC3_crc8(const uint8_t* data, const size_t len);

const SensorModel SHTC3 = {
    .modelname = "SHTC3",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 1000,
    .allocator = unitemp_SHTC3_I2C_alloc,
    .mem_releaser = unitemp_SHTC3_I2C_free,
    .initializer = unitemp_SHTC3_init,
    .deinitializer = unitemp_SHTC3_I2C_deinit,
    .updater = unitemp_SHTC3_I2C_update};

bool unitemp_SHTC3_I2C_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Адреса на шине I2C (7 бит)
    i2c_sensor->min_i2c_adress = 0x70 << 1;
    i2c_sensor->max_i2c_adress = 0x70 << 1;
    return true;
}

bool unitemp_SHTC3_I2C_free(Sensor* sensor) {
    //Нечего высвобождать, так как ничего не было выделено
    UNUSED(sensor);
    return true;
}

bool unitemp_SHTC3_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    if(!SHTC3_soft_reset(i2c_sensor)) return false;
    SHTC3_wakeup(i2c_sensor);

    uint16_t sn = SHTC3_read_serial_number(i2c_sensor);
    UNITEMP_DEBUG("%s serial number %04X", sensor->name, sn);
    if((sn & 0b0000100000111111) != 0b100000000111) return false;

    return true;
}

bool unitemp_SHTC3_I2C_deinit(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    SHTC3_sleep(i2c_sensor);
    return true;
}

SensorStatus unitemp_SHTC3_I2C_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    if(!SHTC3_send_cmd(i2c_sensor, SHTC3_COMMAND_MEASUREMENT_NORMAL_READ_TEMP))
        return UT_SENSORSTATUS_TIMEOUT;
    furi_delay_ms(13); //meauserement timeout

    uint8_t buff[6] = {0};
    if(!unitemp_i2c_read_array(i2c_sensor, 6, buff)) return UT_SENSORSTATUS_TIMEOUT;

    if(unitemp_SHTC3_crc8(buff, 2) != buff[2]) return UT_SENSORSTATUS_BADCRC;
    if(unitemp_SHTC3_crc8(buff + 3, 2) != buff[5]) return UT_SENSORSTATUS_BADCRC;

    uint32_t temp = 175 * ((uint16_t)(buff[0] << 8) | buff[1]);
    uint32_t hum = 100 * ((uint16_t)(buff[3] << 8) | buff[4]);

    sensor->temperature = -45 + temp / 65536.0f;
    sensor->humidity = hum / 65536.0f;

    return UT_SENSORSTATUS_OK;
}

static bool SHTC3_send_cmd(I2CSensor* sensor, uint16_t cmd) {
    uint8_t buff[2] = {(cmd >> 8), cmd & 0xFF};
    return unitemp_i2c_write_array(sensor, 2, buff);
}

static bool SHTC3_soft_reset(I2CSensor* sensor) {
    if(SHTC3_send_cmd(sensor, SHTC3_COMMAND_SOFT_RESET)) {
        furi_delay_us(240); //reset time
        return true;
    } else
        return false;
}

static uint16_t SHTC3_read_serial_number(I2CSensor* sensor) {
    uint8_t buff[2] = {0};
    if(!SHTC3_send_cmd(sensor, SHTC3_COMMAND_READ_SERIAL_NUMBER)) return 0;
    if(!unitemp_i2c_read_array(sensor, 2, buff)) return 0;
    return (buff[0] << 8) | buff[1];
}

static bool SHTC3_wakeup(I2CSensor* sensor) {
    if(SHTC3_send_cmd(sensor, SHTC3_COMMAND_WAKEUP)) {
        furi_delay_us(240); //wakeup time
        return true;
    } else
        return false;
}
static bool SHTC3_sleep(I2CSensor* sensor) {
    return SHTC3_send_cmd(sensor, SHTC3_COMMAND_SLEEP);
}

static uint8_t unitemp_SHTC3_crc8(const uint8_t* data, const size_t len) {
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
