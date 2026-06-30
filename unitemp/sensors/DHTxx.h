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

#include "../sensors.h"

#ifndef DHTXX_H_
#define DHTXX_H_

extern const SensorModel DHT11;
extern const SensorModel DHT21;
extern const SensorModel DHT22;
extern const SensorModel DHT20;
extern const SensorModel AHT10;
extern const SensorModel AHT20;

/**
 * @brief Allocating memory and setting initial values ​​for the DHT20 sensor
 *
 * @param sensor Pointer to the sensor to create
 * @return The truth about success
 */
bool unitemp_DHT20_I2C_alloc(Sensor* sensor, char* args);

/**
 * @brief Initializing the DHT20 sensor
 *
 * @param sensor Pointer to sensor
 * @return True if initialization is successful
 */
bool unitemp_DHT20_init(Sensor* sensor);

/**
 * @brief Deinitializing the sensor
 *
 * @param sensor Pointer to sensor
 */
bool unitemp_DHT20_I2C_deinit(Sensor* sensor);

/**
 * @brief Updating values ​​from sensor
 *
 * @param sensor Pointer to sensor
 * @return Update status
 */
SensorStatus unitemp_DHT20_I2C_update(Sensor* sensor);

/**
 * @brief Free up sensor memory
 *
 * @param sensor Pointer to sensor
 */
bool unitemp_DHT20_I2C_free(Sensor* sensor);
#endif //DHTXX_H_
