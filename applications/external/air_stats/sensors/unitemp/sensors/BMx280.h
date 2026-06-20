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
#ifndef UNITEMP_BMx280
#define UNITEMP_BMx280

#include "../unitemp.h"
#include "../Sensors.h"
#include "../interfaces/I2CSensor.h"

typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
} BMx280_temp_cal;

typedef struct {
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} BMx280_press_cal;

typedef struct {
    uint8_t dig_H1;
    int16_t dig_H2;
    uint8_t dig_H3;
    int16_t dig_H4;
    int16_t dig_H5;
    int8_t dig_H6;
} BMx280_hum_cal;

typedef struct {
    //Temperature calibration values
    BMx280_temp_cal temp_cal;
    //Pressure calibration values
    BMx280_press_cal press_cal;
    //Air humidity calibration values
    BMx280_hum_cal hum_cal;
    //Time of last update of calibration values
    uint32_t last_cal_update_time;
    //Sensor ID
    uint8_t chip_id;
    //Temperature correction value
    int32_t t_fine;
} BMx280_instance;

extern const SensorType BMP280;
extern const SensorType BME280;
/**
 * @brief Allocating memory and setting initial values ​​for the BMP280 sensor
 * @param sensor Pointer to the sensor to create
 * @return The truth about success
 */
bool unitemp_BMx280_alloc(Sensor* sensor, char* args);

/**
 * @brief BMP280 sensor initialization
 * @param sensor Pointer to sensor
 * @return True if initialization is successful
 */
bool unitemp_BMx280_init(Sensor* sensor);

/**
 * @brief Deinitializing the sensor
 * @param sensor Pointer to sensor
 */
bool unitemp_BMx280_deinit(Sensor* sensor);

/**
 * @brief Updating values ​​from sensor
 * @param sensor Pointer to sensor
 * @return Sensor poll status
 */
UnitempStatus unitemp_BMx280_update(Sensor* sensor);

/**
 * @brief Free up sensor memory
 * @param sensor Pointer to sensor
 */
bool unitemp_BMx280_free(Sensor* sensor);

#endif
