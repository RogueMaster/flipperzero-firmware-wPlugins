/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)
    Contributed by divinebird (https://github.com/divinebird)

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

// Some information may be seen on https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library

#include "SCD30.h"
#include "../interfaces/i2c_sensor.h"
#include "../helpers/endianness.h"

const SensorModel SCD30 = {
    .modelname = "SCD30",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM_CO2,
    .polling_interval = 2000,
    .allocator = unitemp_SCD30_alloc,
    .mem_releaser = unitemp_SCD30_free,
    .initializer = unitemp_SCD30_init,
    .deinitializer = unitemp_SCD30_deinit,
    .updater = unitemp_SCD30_update};

typedef union {
    uint16_t array16[2];
    uint8_t array8[4];
    float value;
} ByteToFl;

#define SCD30_I2C_ADDRESS           0x61
#define SCD30_I2C_CMD_TIMEOUT       5 // must be bigger than 3 ms
#define SCD30_PRESSURE_COMPENSATION 0 //Range 700-1400 mBar. 0 deactivates pressure compensation
#define SCD30_MEASUREMENT_INTERVAL  2 //Range 2-1800 sec

#define COMMAND_RESET       0xD304 // Soft reset
#define COMMAND_READ_FW_VER 0xD100 //Read firmware version
#define COMMAND_CONTINUOUS_MEASUREMENT \
    0x0010 //Trigger continuous measurement with optional ambient pressure compensation
#define COMMAND_STOP_MEASUREMENT           0x0104 //Stop continuous measurement
#define COMMAND_SET_MEASUREMENT_INTERVAL   0x4600 //Set measurement interval
#define COMMAND_GET_DATA_READY             0x0202 //Get data ready status
#define COMMAND_AUTOMATIC_SELF_CALIBRATION 0x5306 //(De-)Activate Automatic Self-Calibration (ASC)
#define COMMAND_READ_MEASUREMENT           0x0300 //Read measurement

// #define COMMAND_SET_FORCED_RECALIBRATION_FACTOR 0x5204
// #define COMMAND_SET_TEMPERATURE_OFFSET          0x5403
// #define COMMAND_SET_ALTITUDE_COMPENSATION       0x5102
//

static bool _send_cmd(Sensor* sensor, uint16_t cmd);
static bool _send_cmd_arg_crc(Sensor* sensor, uint16_t cmd, uint16_t arg);
static uint8_t _compute_crc8(uint8_t* message, uint8_t len);

bool scd30_reset(Sensor* sensor);
bool scd30_read_fw_version(Sensor* sensor, uint8_t* major, uint8_t* minor);
bool scd30_start_continuous_measurement(Sensor* sensor, uint16_t pressure_in_mbar);
bool scd30_stop_continuous_measurement(Sensor* sensor);
bool scd30_set_measurement_interval(Sensor* sensor, uint16_t interval_in_sec);
uint16_t scd30_get_measurement_interval(Sensor* sensor);
bool scd30_get_data_ready(Sensor* sensor, bool* state);
bool scd30_set_auto_self_calibration(Sensor* sensor, bool state);
bool scd30_read_measurement(Sensor* sensor);

bool unitemp_SCD30_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    i2c_sensor->min_i2c_adress = SCD30_I2C_ADDRESS << 1;
    i2c_sensor->max_i2c_adress = SCD30_I2C_ADDRESS << 1;
    return true;
}

bool unitemp_SCD30_free(Sensor* sensor) {
    //Nothing to release since nothing was allocated
    UNUSED(sensor);
    return true;
}

bool unitemp_SCD30_init(Sensor* sensor) {
    if(!scd30_reset(sensor)) return false;
    uint8_t major, minor;
    for(uint8_t i = 0; !scd30_read_fw_version(sensor, &major, &minor) && i < 5; i++) {
        furi_delay_ms(50);
    }
    UNITEMP_DEBUG("SCD30 FW v%d.%d", major, minor);

    if(!scd30_start_continuous_measurement(sensor, SCD30_PRESSURE_COMPENSATION)) return false;
    furi_delay_ms(SCD30_I2C_CMD_TIMEOUT);
    if(!scd30_set_measurement_interval(sensor, SCD30_MEASUREMENT_INTERVAL)) return false;
    furi_delay_ms(SCD30_I2C_CMD_TIMEOUT);
    if(!scd30_set_auto_self_calibration(sensor, true)) return false;
    furi_delay_ms(SCD30_I2C_CMD_TIMEOUT);

    return true;
}

bool unitemp_SCD30_deinit(Sensor* sensor) {
    return scd30_stop_continuous_measurement(sensor);
}

SensorStatus unitemp_SCD30_update(Sensor* sensor) {
    bool data_is_ready;
    if(!scd30_get_data_ready(sensor, &data_is_ready)) return UT_SENSORSTATUS_TIMEOUT;
    if(data_is_ready) {
        return scd30_read_measurement(sensor) ? UT_SENSORSTATUS_OK : UT_SENSORSTATUS_ERROR;
    } else {
        return UT_SENSORSTATUS_POLLING;
    }
}

static bool _send_cmd(Sensor* sensor, uint16_t cmd) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    uint8_t buff[2] = {cmd >> 8, cmd};
    return unitemp_i2c_write_array(i2c_sensor, 2, buff);
}
static bool _send_cmd_arg_crc(Sensor* sensor, uint16_t cmd, uint16_t arg) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    uint8_t buff[5] = {cmd >> 8, cmd, arg >> 8, arg};
    uint8_t crc = _compute_crc8(buff + 2, 2);
    buff[4] = crc;

    return unitemp_i2c_write_array(i2c_sensor, 5, buff);
}
static uint16_t _read_reg(Sensor* sensor, uint16_t cmd) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    if(!_send_cmd(sensor, cmd)) return 0xFFFF;
    furi_delay_ms(SCD30_I2C_CMD_TIMEOUT);

    uint8_t buff[3];
    if(!unitemp_i2c_read_array(i2c_sensor, 3, buff)) return 0xFFFF;
    if(buff[2] != _compute_crc8(buff, 2)) return 0xFFFF;

    return (buff[0] << 8) | buff[1];
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
static bool _load_float(uint8_t* buff, float* val) {
    size_t cntr = 0;
    uint8_t floatBuff[4];
    for(size_t i = 0; i < 2; i++) {
        floatBuff[cntr++] = buff[0];
        floatBuff[cntr++] = buff[1];
        uint8_t expectedCRC = _compute_crc8(buff, 2);
        if(buff[2] != expectedCRC) return false;
        buff += 3;
    }
    uint32_t tmpVal = load32_be(floatBuff);
    memcpy(val, &tmpVal, sizeof(float));
    return true;
}

bool scd30_reset(Sensor* sensor) {
    return _send_cmd(sensor, COMMAND_RESET);
}
bool scd30_read_fw_version(Sensor* sensor, uint8_t* major, uint8_t* minor) {
    uint16_t ver = _read_reg(sensor, COMMAND_READ_FW_VER);
    if(ver == 0xFFFF) return false;

    *minor = ver & 0xFF;
    *major = ver >> 8;
    return true;
}
bool scd30_start_continuous_measurement(Sensor* sensor, uint16_t pressure_in_mbar) {
    if(pressure_in_mbar != 0 && (pressure_in_mbar < 700 || pressure_in_mbar > 1400)) {
        return false;
    }
    return _send_cmd_arg_crc(sensor, COMMAND_CONTINUOUS_MEASUREMENT, pressure_in_mbar);
}
bool scd30_stop_continuous_measurement(Sensor* sensor) {
    return _send_cmd(sensor, COMMAND_STOP_MEASUREMENT);
}
bool scd30_set_measurement_interval(Sensor* sensor, uint16_t interval_in_sec) {
    if(interval_in_sec < 2 || interval_in_sec > 1800) {
        return false;
    }
    if(!_send_cmd_arg_crc(sensor, COMMAND_SET_MEASUREMENT_INTERVAL, interval_in_sec)) return false;

    furi_delay_ms(SCD30_I2C_CMD_TIMEOUT);

    uint16_t readed_interval = scd30_get_measurement_interval(sensor);
    if(readed_interval == interval_in_sec)
        return true;
    else {
        FURI_LOG_E(
            APP_NAME,
            "Measure interval set wrong! Readed %ds, expected %ds",
            readed_interval,
            interval_in_sec);
        return false;
    }
}
uint16_t scd30_get_measurement_interval(Sensor* sensor) {
    return _read_reg(sensor, COMMAND_SET_MEASUREMENT_INTERVAL);
}
bool scd30_set_auto_self_calibration(Sensor* sensor, bool state) {
    return _send_cmd_arg_crc(sensor, COMMAND_AUTOMATIC_SELF_CALIBRATION, state ? 1 : 0);
}
bool scd30_get_data_ready(Sensor* sensor, bool* state) {
    uint16_t reg = _read_reg(sensor, COMMAND_GET_DATA_READY);
    if(reg == 0xFFFF) {
        *state = false;
        return false;
    } else {
        *state = reg == 1;
        return true;
    }
}
bool scd30_read_measurement(Sensor* sensor) {
    _send_cmd(sensor, COMMAND_READ_MEASUREMENT);
    furi_delay_ms(SCD30_I2C_CMD_TIMEOUT);
    uint8_t buff[18];
    if(!unitemp_i2c_read_array(sensor->instance, 18, buff)) return false;

    bool error = false;
    float tempCO2 = 0;
    float tempHumidity = 0;
    float tempTemperature = 0;
    if(_load_float(buff, &tempCO2)) {
        sensor->co2 = tempCO2;
    } else {
        FURI_LOG_E(APP_NAME, "Error while parsing CO2");
        error = true;
    };
    if(_load_float(buff + 6, &tempTemperature)) {
        sensor->temperature = tempTemperature;
    } else {
        FURI_LOG_E(APP_NAME, "Error while parsing temp");
        error = true;
    }

    if(_load_float(buff + 12, &tempHumidity)) {
        sensor->humidity = tempHumidity;
    } else {
        FURI_LOG_E(APP_NAME, "Error while parsing humidity");
        error = true;
    }

    return !error;
}
