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

#ifndef DS18X2X_H_
#define DS18X2X_H_

extern const SensorModel Dallas;

/**
 * @brief Memory allocation for sensor on OneWire bus
 * @param sensor Pointer to sensor
 * @param args Pointer to an array of arguments with sensor parameters
 * @return Istina if all ok
 */
bool unitemp_ds18x2x_sensor_alloc(Sensor* sensor, char* args);

/**
 * @brief Freeing sensor instance memory
 * @param sensor Pointer to sensor
 */
bool unitemp_ds18x2x_sensor_free(Sensor* sensor);

/**
 * @brief Initializing the sensor on the one wire bus
 * @param sensor Pointer to sensor
 * @return True if initialization is successful
 */
bool unitemp_ds18x2x_sensor_init(Sensor* sensor);

/**
 * @brief Deinitializing the sensor
 * @param sensor Pointer to sensor
 */
bool unitemp_ds18x2x_sensor_deinit(Sensor* sensor);

/**
 * @brief Update value from sensor
 * @param sensor Pointer to sensor
 * @return Update status
 */
SensorStatus unitemp_ds18x2x_sensor_update(Sensor* sensor);

#endif //DS18X2X_H_
