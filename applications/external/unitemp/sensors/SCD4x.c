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

#include "SCD4x.h"
#include "../interfaces/i2c_sensor.h"

const SensorModel SCD4x = {
    .modelname = "SCD4x",
    .altname = "SCD40/SCD41",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM_CO2,
    .polling_interval = 5000,
    .allocator = unitemp_SCD4x_alloc,
    .mem_releaser = unitemp_SCD4x_free,
    .initializer = unitemp_SCD4x_init,
    .deinitializer = unitemp_SCD4x_deinit,
    .updater = unitemp_SCD4x_update};

#define SCD4x_I2C_ADDRESS 0x62

#define COMMAND_GET_AUTO_SELF_CALIB_EN        0x2313
#define COMMAND_GET_DATA_READY_STATUS         0xE4B8 //ok
#define COMMAND_GET_TEMPERATURE_OFFSET        0x2318
#define COMMAND_GET_SENSOR_ALTITUDE           0x2322
#define COMMAND_GET_SERIAL_NUMBER             0x3682 //ok
#define COMMAND_MEASURE_SINGLE_SHOT           0x219D
#define COMMAND_MEASURE_SINGLE_SHOT_RHT_ONLY  0x2196
#define COMMAND_PERFORM_FORCED_RECALIBRATION  0x362F
#define COMMAND_PERFORM_SELF_TEST             0x3639
#define COMMAND_PERSIST_SETTINGS              0x3615
#define COMMAND_PERFORM_FACTORY_RESET         0x3632
#define COMMAND_READ_MEASUREMENT              0xEC05 //ok
#define COMMAND_REINIT                        0x3646
#define COMMAND_START_PERIODIC_MEASUREMENT    0x21B1 //ok
#define COMMAND_START_LOW_POWER_PERIODIC_MEAS 0x21AC
#define COMMAND_STOP_PERIODIC_MEASUREMENT     0x3F86 //ok
#define COMMAND_SET_TEMPERATURE_OFFSET        0x241D
#define COMMAND_SET_SENSOR_ALTITUDE           0x2427
#define COMMAND_SET_AMBIENT_PRESSURE          0xE000
#define COMMAND_SET_AUTO_SELF_CALIB_EN        0x2416

#define COMMAND_POWER_DOWN 0x36E0
#define COMMAND_WAKE_UP    0x36F6

bool SCD4x_start_periodic_measurement(Sensor* sensor);
bool SCD4x_read_measurement(Sensor* sensor);
bool SCD4x_stop_periodic_measurement(Sensor* sensor);

bool SCD4x_get_data_ready_status(Sensor* sensor);
uint64_t SCD4x_get_serial_number(Sensor* sensor);

static bool _send_cmd(Sensor* sensor, uint16_t cmd);
static uint8_t _compute_crc8(uint8_t* message, uint8_t len);
//static bool _send_cmd_arg_crc(Sensor* sensor, uint16_t cmd, uint16_t arg);

bool unitemp_SCD4x_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    i2c_sensor->min_i2c_adress = SCD4x_I2C_ADDRESS << 1;
    i2c_sensor->max_i2c_adress = SCD4x_I2C_ADDRESS << 1;
    return true;
}

bool unitemp_SCD4x_free(Sensor* sensor) {
    //Nothing to release since nothing was allocated
    UNUSED(sensor);
    return true;
}

bool unitemp_SCD4x_init(Sensor* sensor) {
    uint64_t serial_number = SCD4x_get_serial_number(sensor);
    if(!serial_number) return false;
    UNITEMP_DEBUG("%s serial number %06llX", sensor->name, serial_number);

    if(!SCD4x_start_periodic_measurement(sensor)) return false;

    return true;
}

bool unitemp_SCD4x_deinit(Sensor* sensor) {
    return SCD4x_stop_periodic_measurement(sensor);
}

SensorStatus unitemp_SCD4x_update(Sensor* sensor) {
    if(!SCD4x_get_data_ready_status(sensor)) return UT_SENSORSTATUS_TIMEOUT;

    return SCD4x_read_measurement(sensor) ? UT_SENSORSTATUS_OK : UT_SENSORSTATUS_TIMEOUT;
}

static uint8_t _compute_crc8(uint8_t* message, uint8_t len) {
    uint8_t crc = 0xFF; // Init with 0xFF
    for(uint8_t x = 0; x < len; x++) {
        crc ^= message[x]; // XOR-in the next input byte
        for(uint8_t i = 0; i < 8; i++) {
            if((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ 0x31);
            else
                crc <<= 1;
        }
    }
    return crc; // No output reflection
}

static bool _send_cmd(Sensor* sensor, uint16_t cmd) {
    I2CSensor* i2c_sensor = sensor->instance;
    uint8_t buff[2] = {cmd >> 8, cmd & 0x00FF};
    return unitemp_i2c_write_array(i2c_sensor, 2, buff);
}

// static bool _send_cmd_arg_crc(Sensor* sensor, uint16_t cmd, uint16_t arg) {
//     I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

//     uint8_t buff[5] = {cmd >> 8, cmd, arg >> 8, arg};
//     uint8_t crc = _compute_crc8(buff + 2, 2);
//     buff[4] = crc;

//     return unitemp_i2c_write_array(i2c_sensor, 5, buff);
// }

bool SCD4x_start_periodic_measurement(Sensor* sensor) {
    return _send_cmd(sensor, COMMAND_START_PERIODIC_MEASUREMENT);
}

bool SCD4x_read_measurement(Sensor* sensor) {
    I2CSensor* i2c_sensor = sensor->instance;

    //Send read cmd
    if(!_send_cmd(sensor, COMMAND_READ_MEASUREMENT)) return false;
    furi_delay_ms(1); //Mandatory delay during command execution by the sensor

    //Reading raw data
    uint8_t buff[9] = {0};
    if(!unitemp_i2c_read_array(i2c_sensor, 9, buff)) return false;

    //Checking CRC
    if(_compute_crc8(buff + 0, 2) != buff[2]) return false;
    if(_compute_crc8(buff + 3, 2) != buff[5]) return false;
    if(_compute_crc8(buff + 6, 2) != buff[8]) return false;

    //Converting values
    sensor->co2 = (buff[0] << 8) | buff[1];
    sensor->temperature = -45.0f + 175.0f * ((buff[3] << 8) | buff[4]) / 65535.0f;
    sensor->humidity = 100.0f * ((buff[3] << 8) | buff[4]) / 65535.0f;

    return true;
}

bool SCD4x_stop_periodic_measurement(Sensor* sensor) {
    bool result = _send_cmd(sensor, COMMAND_STOP_PERIODIC_MEASUREMENT);
    if(result) furi_delay_ms(500);
    return result;
}

bool SCD4x_get_data_ready_status(Sensor* sensor) {
    I2CSensor* i2c_sensor = sensor->instance;

    if(!_send_cmd(sensor, COMMAND_GET_DATA_READY_STATUS)) return false;
    furi_delay_ms(1);

    uint8_t buff[3] = {0};
    if(!unitemp_i2c_read_array(i2c_sensor, 3, buff)) return false;
    if(_compute_crc8(buff + 0, 2) != buff[2]) return false;

    uint16_t responce = ((uint16_t)buff[0] << 8) | buff[1];
    if((responce & 0x8000) != 0x8000) return false; //wrong responce
    if((responce & ~0x8000) == 0x0000) return false; //data not ready
    return true;
}

uint64_t SCD4x_get_serial_number(Sensor* sensor) {
    I2CSensor* i2c_sensor = sensor->instance;

    if(!_send_cmd(sensor, COMMAND_GET_SERIAL_NUMBER)) return 0;
    furi_delay_ms(1);

    uint8_t buff[9] = {0};
    unitemp_i2c_read_array(i2c_sensor, 9, buff);

    //Checking CRC
    if(_compute_crc8(buff + 0, 2) != buff[2]) return 0;
    if(_compute_crc8(buff + 3, 2) != buff[5]) return 0;
    if(_compute_crc8(buff + 6, 2) != buff[8]) return 0;

    uint16_t fw = ((uint16_t)buff[0] << 8) | buff[1];
    uint16_t sw = ((uint16_t)buff[3] << 8) | buff[4];
    uint16_t tw = ((uint16_t)buff[6] << 8) | buff[7];
    return (((uint64_t)fw << 32) | ((uint64_t)sw << 16) | tw);
}
