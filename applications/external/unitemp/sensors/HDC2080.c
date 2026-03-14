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
#include "HDC2080.h"
#include "../interfaces/i2c_sensor.h"

#define TEMPERATURE_LOW      0x00 // Temperature [7:0]
#define TEMPERATURE_HIGH     0x01 // Temperature [15:8]
#define HUMIDITY_LOW         0x02 // Humidity [7:0]
#define HUMIDITY_HIGH        0x03 // Humidity [15:8]
#define INTERRUPT_DRDY       0x04 // DataReady and interrupt configuration
#define TEMPERATURE_MAX      0x05 // Maximum measured temperature (Not supported in AM Mode)
#define HUMIDITY_MAX         0x06 // Maximum measured humidity (Not supported in AM Mode)
#define INTERRUPT_ENABLE     0x07 // Interrupt Enable
#define TEMP_OFFSET_ADJUST   0x08 // Temperature value adjustment
#define HUM_OFFSET_ADJUST    0x09 // Humidity value adjustment
#define TEMP_THR_L           0x0A // Temperature Threshold Low
#define TEMP_THR_H           0x0B // Temperature Threshold High
#define RH_THR_L             0x0C // Humidity threshold Low
#define RH_THR_H             0x0D // Humidity threshold High
#define RESET_DRYD_INT_CONF  0x0E // Soft Reset and Interrupt Configuration
#define MEASUREMENT_CONFIG   0x0F // Measurement configuration
#define MANUFACTURER_ID_LOW  0xFC // Manufacturer ID Low
#define MANUFACTURER_ID_HIGH 0xFD // Manufacturer ID High
#define DEVICE_ID_LOW        0xFE // Device ID Low
#define DEVICE_ID_HIGH       0xFF // Device ID High

#define HDC2080_FACTORY_DEVICE_ID 0x07D0

// Auto Measurement Mode period
typedef enum {
    AMM_DISABLED = 0b000, // Disabled. Initiate measurement via I2C
    AMM_PERIOD_120000_MS = 0b001, // 1/120 Hz (120 000 ms)
    AMM_PERIOD_60000_MS = 0b010, // 1/60 Hz (60 000 ms)
    AMM_PERIOD_10000_MS = 0b011, // 0.1 Hz (10 000 ms)
    AMM_PERIOD_5000_MS = 0b100, // 0.2 Hz (5 000 ms)
    AMM_PERIOD_1000_MS = 0b101, // 1 Hz (1 000 ms)
    AMM_PERIOD_500_MS = 0b110, // 2 Hz (500 ms)
    AMM_PERIOD_200_MS = 0b111, // 5 Hz (200 ms)
} HDC2080_AMMPeriod;

// Configuration Register
typedef struct {
    int INT_MODE    : 1; //[0] Interrupt mode. 0 = Level sensitive. 1 = Comparator mode
    int INT_POL     : 1; //[1] Interrupt polarity. 0 = Active Low, 1 = Active High
    int DRDY_INT_EN : 1; //[2] DRDY/INT_EN pin configuration. 0 = High Z, 1 = Enable
    int HEAT_EN     : 1; //[3] 0 = Heater off, 1 = Heater on
    int AMM_PERIOD  : 3; //[4:6] Auto Measurement Mode (AMM). See HDC2080_AMMPeriod
    int SOFT_RES    : 1; //[7] 0 = Normal Operation mode, 1 soft reset
} HDC2080_CR;

typedef enum {
    MEASUREMENT_RESOLUTION_14_BIT = 0b00,
    MEASUREMENT_RESOLUTION_11_BIT = 0b01,
    MEASUREMENT_RESOLUTION_9_BIT = 0b10,
} HDC2080_MeasurementResolution;

typedef enum {
    MEAS_CONF_HUM_TEMP = 0b00, // Humidity + Temperature
    MEAS_CONF_TEMP = 0b01, // Temperature only
} HDC2080_MeasurementMode;

// Measurement Configuration Register
typedef struct {
    unsigned int MEAS_TRIG : 1; //[0] Measurement trigger. 0: no action, 1: Start measurement
    unsigned int MEAS_CONF : 2; //[2:1] Measurement configuration. See HDC2080_MeasurementMode
    unsigned int RES       : 1; //[3] Reserved
    unsigned int HRES      : 2; //[5:4] Humidity resolution. See HDC2080_MeasurementResolution
    unsigned int TRES      : 2; //[7:6] Temperature resolution. See HDC2080_MeasurementResolution
} HDC2080_MCR;

// Interrupt Configuration Register
typedef struct {
    unsigned int RES         : 3; // [2:0] Reserved
    unsigned int HL_ENABLE   : 1; // [3] Humidity threshold LOW Interrupt enable
    unsigned int HH_ENABLE   : 1; // [4] Humidity threshold HIGH Interrupt enable
    unsigned int TL_ENABLE   : 1; // [5] Temperature threshold LOW Interrupt enable
    unsigned int TH_ENABLE   : 1; // [6] Temperature threshold HIGH Interrupt enable
    unsigned int DRDY_ENABLE : 1; // [7] DataReady Interrupt enable
} HDC2080_ICR;

typedef struct {
    HDC2080_CR config_reg;
    HDC2080_MCR meas_config_reg;
    HDC2080_ICR int_conf_reg;
} HDC2080Sensor;

const SensorModel HDC2080 = {
    .modelname = "HDC2080",
    .interface = &unitemp_i2c,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 250,
    .allocator = unitemp_HDC2080_alloc,
    .mem_releaser = unitemp_HDC2080_free,
    .initializer = unitemp_HDC2080_init,
    .deinitializer = unitemp_HDC2080_deinit,
    .updater = unitemp_HDC2080_update};

void HDC2080_read_meas_conf_reg(I2CSensor* i2c_sensor);
void HDC2080_write_meas_conf_reg(I2CSensor* i2c_sensor);
void HDC2080_read_conf_reg(I2CSensor* i2c_sensor);
void HDC2080_write_conf_reg(I2CSensor* i2c_sensor);
void HDC2080_read_int_conf_reg(I2CSensor* i2c_sensor);
void HDC2080_write_int_conf_reg(I2CSensor* i2c_sensor);

uint16_t HDC2080_get_device_id(I2CSensor* i2c_sensor);
float HDC2080_get_temperature(I2CSensor* i2c_sensor);
float HDC2080_get_humidity(I2CSensor* i2c_sensor);

void HDC2080_set_amm_period(HDC2080Sensor* hdc2080_sensor, HDC2080_AMMPeriod period);
void HDC2080_set_heater(HDC2080Sensor* hdc2080_sensor, bool state);
void HDC2080_set_temp_resolution(HDC2080Sensor* hdc2080_sensor, HDC2080_MeasurementResolution res);
void HDC2080_set_hum_resolution(HDC2080Sensor* hdc2080_sensor, HDC2080_MeasurementResolution res);
void HDC2080_set_meas_mode(HDC2080Sensor* hdc2080_sensor, HDC2080_MeasurementMode mode);
void HDC2080_set_meas_trig(HDC2080Sensor* hdc2080_sensor, bool state);
void HDC2080_set_interrupt_drdy(HDC2080Sensor* hdc2080_sensor, bool state);
void HDC2080_set_interrupt_th(HDC2080Sensor* hdc2080_sensor, bool state);
void HDC2080_set_interrupt_tl(HDC2080Sensor* hdc2080_sensor, bool state);
void HDC2080_set_interrupt_hh(HDC2080Sensor* hdc2080_sensor, bool state);
void HDC2080_set_interrupt_hl(HDC2080Sensor* hdc2080_sensor, bool state);
bool HDC2080_set_temp_offset(I2CSensor* i2c_sensor, float offset);
bool HDC2080_set_hum_offset(I2CSensor* i2c_sensor, float offset);

bool HDC2080_is_data_ready(I2CSensor* i2c_sensor);

bool unitemp_HDC2080_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Addresses on the I2C bus (7 bits)
    i2c_sensor->min_i2c_adress = 0x40 << 1;
    i2c_sensor->max_i2c_adress = 0x41 << 1;

    HDC2080Sensor* hdc2080_sensor = malloc(sizeof(HDC2080Sensor));
    if(hdc2080_sensor != NULL) {
        i2c_sensor->sensor_instance = hdc2080_sensor;
        return true;
    } else {
        return false;
    }
}

bool unitemp_HDC2080_free(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    HDC2080Sensor* hdc2080_sensor = i2c_sensor->sensor_instance;

    free(hdc2080_sensor);

    return true;
}

bool unitemp_HDC2080_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    HDC2080Sensor* hdc2080_sensor = i2c_sensor->sensor_instance;

    uint16_t device_id = HDC2080_get_device_id(i2c_sensor);
    if(device_id != HDC2080_FACTORY_DEVICE_ID) {
        FURI_LOG_E(
            APP_NAME,
            "Sensor %s returned wrong ID 0x%04X, expected 0x%04X",
            sensor->name,
            device_id,
            HDC2080_FACTORY_DEVICE_ID);
        return false;
    }
    UNITEMP_DEBUG("HDC2080 Device ID: %04X", device_id);

    HDC2080_read_conf_reg(i2c_sensor);
    HDC2080_set_amm_period(hdc2080_sensor, AMM_PERIOD_200_MS);
    HDC2080_set_heater(hdc2080_sensor, false);
    HDC2080_write_conf_reg(i2c_sensor);

    HDC2080_read_int_conf_reg(i2c_sensor);
    HDC2080_set_interrupt_drdy(hdc2080_sensor, true);
    HDC2080_write_int_conf_reg(i2c_sensor);

    HDC2080_read_meas_conf_reg(i2c_sensor);
    HDC2080_set_temp_resolution(hdc2080_sensor, MEASUREMENT_RESOLUTION_14_BIT);
    HDC2080_set_hum_resolution(hdc2080_sensor, MEASUREMENT_RESOLUTION_14_BIT);
    HDC2080_set_meas_mode(hdc2080_sensor, MEAS_CONF_HUM_TEMP);
    HDC2080_set_meas_trig(hdc2080_sensor, true);
    HDC2080_write_meas_conf_reg(i2c_sensor);

    return true;
}

bool unitemp_HDC2080_deinit(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    UNUSED(i2c_sensor);
    return true;
}

SensorStatus unitemp_HDC2080_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    if(!HDC2080_is_data_ready(i2c_sensor)) return UT_SENSORSTATUS_TIMEOUT;
    sensor->temperature = HDC2080_get_temperature(i2c_sensor);
    sensor->humidity = HDC2080_get_humidity(i2c_sensor);

    return UT_SENSORSTATUS_OK;
}

void HDC2080_read_meas_conf_reg(I2CSensor* i2c_sensor) {
    HDC2080Sensor* hdc2080_sensor = i2c_sensor->sensor_instance;
    uint8_t reg = unitemp_i2c_read_reg(i2c_sensor, MEASUREMENT_CONFIG);
    memcpy(&(hdc2080_sensor->meas_config_reg), &reg, sizeof(uint8_t));
    UNITEMP_DEBUG("readed MCR: %02X", reg);
}

void HDC2080_write_meas_conf_reg(I2CSensor* i2c_sensor) {
    HDC2080Sensor* hdc2080_sensor = i2c_sensor->sensor_instance;
    uint8_t reg = *(uint8_t*)&hdc2080_sensor->meas_config_reg;
    unitemp_i2c_write_reg(i2c_sensor, MEASUREMENT_CONFIG, reg);
    UNITEMP_DEBUG("written MCR: %02X", reg);
}

void HDC2080_read_conf_reg(I2CSensor* i2c_sensor) {
    HDC2080Sensor* hdc2080_sensor = i2c_sensor->sensor_instance;
    uint8_t reg = unitemp_i2c_read_reg(i2c_sensor, RESET_DRYD_INT_CONF);
    memcpy(&(hdc2080_sensor->config_reg), &reg, sizeof(uint8_t));
    UNITEMP_DEBUG("readed CR: %02X", reg);
}

void HDC2080_write_conf_reg(I2CSensor* i2c_sensor) {
    HDC2080Sensor* hdc2080_sensor = i2c_sensor->sensor_instance;
    uint8_t reg = *(uint8_t*)&hdc2080_sensor->config_reg;
    unitemp_i2c_write_reg(i2c_sensor, RESET_DRYD_INT_CONF, reg);
    UNITEMP_DEBUG("written CR: %02X", reg);
}

void HDC2080_read_int_conf_reg(I2CSensor* i2c_sensor) {
    HDC2080Sensor* hdc2080_sensor = i2c_sensor->sensor_instance;
    uint8_t reg = unitemp_i2c_read_reg(i2c_sensor, INTERRUPT_ENABLE);
    memcpy(&(hdc2080_sensor->int_conf_reg), &reg, sizeof(uint8_t));
    UNITEMP_DEBUG("readed ICR: %02X", reg);
}

void HDC2080_write_int_conf_reg(I2CSensor* i2c_sensor) {
    HDC2080Sensor* hdc2080_sensor = i2c_sensor->sensor_instance;
    uint8_t reg = *(uint8_t*)&hdc2080_sensor->int_conf_reg;
    unitemp_i2c_write_reg(i2c_sensor, INTERRUPT_ENABLE, reg);
    UNITEMP_DEBUG("written ICR: %02X", reg);
}

uint16_t HDC2080_get_device_id(I2CSensor* i2c_sensor) {
    uint8_t data[2] = {0};
    unitemp_i2c_read_reg_array(i2c_sensor, DEVICE_ID_LOW, 2, data);
    return ((uint16_t)data[1] << 8) | data[0];
}

float HDC2080_get_temperature(I2CSensor* i2c_sensor) {
    uint8_t data[2] = {0};
    unitemp_i2c_read_reg_array(i2c_sensor, TEMPERATURE_LOW, 2, data);

    return ((float)(((uint16_t)data[1] << 8) | data[0]) / 65536) * 165 - 40;
}

float HDC2080_get_humidity(I2CSensor* i2c_sensor) {
    uint8_t data[2] = {0};
    unitemp_i2c_read_reg_array(i2c_sensor, HUMIDITY_LOW, 2, data);

    return ((float)(((uint16_t)data[1] << 8) | data[0]) / 65536) * 100;
}

void HDC2080_set_amm_period(HDC2080Sensor* hdc2080_sensor, HDC2080_AMMPeriod period) {
    hdc2080_sensor->config_reg.AMM_PERIOD = period;
}

void HDC2080_set_heater(HDC2080Sensor* hdc2080_sensor, bool state) {
    hdc2080_sensor->config_reg.HEAT_EN = state ? 1 : 0;
}

void HDC2080_set_temp_resolution(HDC2080Sensor* hdc2080_sensor, HDC2080_MeasurementResolution res) {
    hdc2080_sensor->meas_config_reg.TRES = res;
}

void HDC2080_set_hum_resolution(HDC2080Sensor* hdc2080_sensor, HDC2080_MeasurementResolution res) {
    hdc2080_sensor->meas_config_reg.HRES = res;
}

void HDC2080_set_meas_mode(HDC2080Sensor* hdc2080_sensor, HDC2080_MeasurementMode mode) {
    hdc2080_sensor->meas_config_reg.MEAS_CONF = mode;
}

void HDC2080_set_interrupt_drdy(HDC2080Sensor* hdc2080_sensor, bool state) {
    hdc2080_sensor->int_conf_reg.DRDY_ENABLE = state ? 1 : 0;
}

void HDC2080_set_interrupt_th(HDC2080Sensor* hdc2080_sensor, bool state) {
    hdc2080_sensor->int_conf_reg.TH_ENABLE = state ? 1 : 0;
}

void HDC2080_set_interrupt_tl(HDC2080Sensor* hdc2080_sensor, bool state) {
    hdc2080_sensor->int_conf_reg.TL_ENABLE = state ? 1 : 0;
}

void HDC2080_set_interrupt_hh(HDC2080Sensor* hdc2080_sensor, bool state) {
    hdc2080_sensor->int_conf_reg.HH_ENABLE = state ? 1 : 0;
}

void HDC2080_set_interrupt_hl(HDC2080Sensor* hdc2080_sensor, bool state) {
    hdc2080_sensor->int_conf_reg.HL_ENABLE = state ? 1 : 0;
}

bool HDC2080_is_data_ready(I2CSensor* i2c_sensor) {
    uint8_t reg = unitemp_i2c_read_reg(i2c_sensor, INTERRUPT_DRDY);
    return (reg & (1 << 7)) ? true : false;
}

void HDC2080_set_meas_trig(HDC2080Sensor* hdc2080_sensor, bool state) {
    hdc2080_sensor->meas_config_reg.MEAS_TRIG = state ? 1 : 0;
}
