/*
    Unitemp - Universal temperature reader
    Copyright (C) 2025  Jakub Kakona (https://github.com/kaklik)

    This file declares the interfaces for using the Maxim Integrated
    MAX31725 temperature sensor over I2C. The sensor provides high
    resolution (0.00390625 °C, Q8.8 format) local temperature measurement,
    overtemperature alarm, and low-power shutdown modes.
  
    Datasheet: https://datasheets.maximintegrated.com/en/ds/MAX31725-MAX31726.pdf
 
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
  
#ifndef UNITEMP_MAX31725
#define UNITEMP_MAX31725

/**
 * @file MAX31725.h
 * @brief Driver declarations for the MAX31725 temperature sensor (I2C interface)
 *
 * Provides function prototypes and sensor type definition for interfacing
 * with the MAX31725 high-resolution temperature sensor within the Unitemp framework.
 */

#include "../unitemp.h"
#include "../Sensors.h"


/**
 * @brief SensorType instance for the MAX31725 sensor
 *
 * Registers the MAX31725 with Unitemp, specifying the I2C interface,
 * data type (temperature), and associated callbacks.
 */
extern const SensorType MAX31725;

/**
 * @brief Allocate and configure MAX31725 sensor instance
 *
 * Sets the valid I2C address range (0x48–0x4F) based on A0–A2 pins.
 *
 * @param sensor Pointer to the Sensor object to initialize
 * @param args Unused initialization arguments
 * @return true if allocation succeeded, false otherwise
 */
bool unitemp_MAX31725_alloc(Sensor* sensor, char* args);

/**
 * @brief Release resources for MAX31725 sensor instance
 *
 * Currently does nothing, provided for API consistency.
 *
 * @param sensor Pointer to the Sensor object
 * @return true always
 */
bool unitemp_MAX31725_free(Sensor* sensor);

/**
 * @brief Initialize MAX31725 sensor hardware
 *
 * Configures sensor for comparator mode, one-fault queue,
 * normal data format, OS active-low, and I2C timeout enabled.
 *
 * @param sensor Pointer to the Sensor object
 * @return true on successful I2C write, false on error
 */
bool unitemp_MAX31725_init(Sensor* sensor);

/**
 * @brief Deinitialize MAX31725 sensor hardware
 *
 * Puts the sensor into shutdown mode to reduce power consumption.
 *
 * @param sensor Pointer to the Sensor object
 * @return true on successful I2C write, false on error
 */
bool unitemp_MAX31725_deinit(Sensor* sensor);

/**
 * @brief Read temperature from MAX31725 sensor
 *
 * Reads two bytes from the temperature register (Q8.8 format)
 * and converts to Celsius degrees.
 *
 * @param sensor Pointer to the Sensor object
 * @return UT_SENSORSTATUS_OK on success,
 *         UT_SENSORSTATUS_TIMEOUT on I2C read failure
 */
UnitempStatus unitemp_MAX31725_update(Sensor* sensor);

#endif // UNITEMP_MAX31725

