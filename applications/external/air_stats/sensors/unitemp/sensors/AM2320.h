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
#ifndef UNITEMP_AM2320
#define UNITEMP_AM2320

#include "../unitemp.h"
#include "../Sensors.h"
extern const SensorType AM2320_I2C;
/**
 * @brief Allocating memory and setting initial values ​​for the AM2320 sensor
 *
 * @param sensor Pointer to the sensor to create
 * @return The truth about success
 */
bool unitemp_AM2320_I2C_alloc(Sensor* sensor, char* args);

/**
 * @brief AM2320 sensor initialization
 *
 * @param sensor Pointer to sensor
 * @return True if initialization is successful
 */
bool unitemp_AM2320_init(Sensor* sensor);

/**
 * @brief Deinitializing the sensor
 *
 * @param sensor Pointer to sensor
 */
bool unitemp_AM2320_I2C_deinit(Sensor* sensor);

/**
 * @brief Updating values ​​from sensor
 *
 * @param sensor Pointer to sensor
 * @return Update status
 */
UnitempStatus unitemp_AM2320_I2C_update(Sensor* sensor);

/**
 * @brief Free up sensor memory
 *
 * @param sensor Pointer to sensor
 */
bool unitemp_AM2320_I2C_free(Sensor* sensor);

#endif
