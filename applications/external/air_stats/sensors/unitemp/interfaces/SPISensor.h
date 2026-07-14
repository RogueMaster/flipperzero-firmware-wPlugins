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
#ifndef UNITEMP_SPI
#define UNITEMP_SPI

#include "../unitemp.h"
#include <furi_hal_spi.h>

//SPI sensor structure
typedef struct SPISensor {
    //Pointer to SPI interface
    FuriHalSpiBusHandle* spi;
    //CS connection port
    const GPIO* CS_pin;
} SPISensor;

/**
 * @brief Memory allocation for SPI sensor
 * @param sensor Pointer to sensor
 * @param args Pointer to an array of arguments with sensor parameters
 * @return Istina if all ok
 */
bool unitemp_spi_sensor_alloc(Sensor* sensor, char* args);

/**
 * @brief Freeing sensor instance memory
 * @param sensor Pointer to sensor
 */
bool unitemp_spi_sensor_free(Sensor* sensor);

/**
 * @brief Initializing a sensor with a one wire interface
 * @param sensor Pointer to sensor
 * @return True if initialization is successful
 */
bool unitemp_spi_sensor_init(Sensor* sensor);

/**
 * @brief Deinitializing the sensor
 * @param sensor Pointer to sensor
 */
bool unitemp_spi_sensor_deinit(Sensor* sensor);

/**
 * @brief Update value from sensor
 * @param sensor Pointer to sensor
 * @return Update status
 */
UnitempStatus unitemp_spi_sensor_update(Sensor* sensor);

#endif
