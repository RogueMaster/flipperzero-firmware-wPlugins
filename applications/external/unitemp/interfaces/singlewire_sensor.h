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
#ifndef singlewire_H_
#define singlewire_H_

#include "../unitemp.h"
#include "../sensors.h"
#include "../helpers/unitemp_gpio.h"

//Single Wire Interface stcructure
typedef struct {
    //Sensor connection port
    const SensorGpioPin* data_pin;
} SingleWireSensor;

extern const SensorConnectionInterface
    unitemp_singlewire; //Proprietary single-wire protocol for DHTXX and AM23XX sensors

/**
 * @brief Memory allocation for a sensor on a One Wire line
 * 
 * @param sensor Pointer to sensor
 * @param args Index on an array with the arguments of the parameters
 * @return true if everything went well
 */
bool unitemp_singlewire_alloc(Sensor* sensor, char* args);

/**
 * @brief Freeing sensor instance memory
 * 
 * @param sensor Pointer to sensor
 * @return true if everything went well
 */
bool unitemp_singlewire_free(Sensor* sensor);

/**
 * @brief Sensor initialization
 * 
 * @param sensor Pointer to the sensor to be initialized
 * @return true if everything went well
 */
bool unitemp_singlewire_init(Sensor* sensor);

/**
 * @brief Deinitializing the sensor
 * 
 * @param sensor Pointer to the sensor to be initialized
 * @return true if everything went well
 */
bool unitemp_singlewire_deinit(Sensor* sensor);

/**
 * @brief Receiving data from the sensor via single-wire interface DHTxx and AM2xxx
 * 
 * @param sensor Pointer to sensor
 * @return Poll status
 */
SensorStatus unitemp_singlewire_update(Sensor* sensor);

/**
 * @brief Set sensor port
 * 
 * @param sensor Pointer to sensor
 * @param data_pin Port to set
 * @return true if all ok
 */
bool unitemp_singlewire_sensor_gpio_set(Sensor* sensor, const SensorGpioPin* data_pin);

/**
 * @brief Get sensor port
 * 
 * @param sensor Pointer to sensor
 * @return Pointer to SensorGpioPin
 */
const SensorGpioPin* unitemp_singlewire_sensor_gpio_get(Sensor* sensor);

#endif // singlewire_H_
