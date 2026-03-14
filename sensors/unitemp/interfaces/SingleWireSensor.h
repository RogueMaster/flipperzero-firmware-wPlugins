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
#ifndef UNITEMP_SINGLE_WIRE
#define UNITEMP_SINGLE_WIRE

#include "../unitemp.h"
#include "../Sensors.h"

//Single Wire Interface
typedef struct {
    //Sensor connection port
    const GPIO* gpio;
} SingleWireSensor;

/* Sensors */
extern const SensorType DHT11;
extern const SensorType DHT12_SW;
extern const SensorType DHT21;
extern const SensorType DHT22;
extern const SensorType AM2320_SW;

/**
 * @brief Sensor initialization
 * 
 * @param sensor Pointer to the sensor to be initialized
 * @return True if everything went well
 */
bool unitemp_singlewire_init(Sensor* sensor);

/**
 * @brief Deinitializing the sensor
 * 
 * @param sensor Pointer to the sensor to be initialized
 * @return True if everything went well
 */
bool unitemp_singlewire_deinit(Sensor* sensor);

/**
 * @brief Receiving data from the sensor via single-wire interface DHTxx and AM2xxx
 * 
 * @param sensor Pointer to sensor
 * @return Poll status
 */
UnitempStatus unitemp_singlewire_update(Sensor* sensor);

/**
 * @brief Set sensor port
 * 
 * @param sensor Pointer to sensor
 * @param gpio Port to set
 * @return Istina if all ok
 */
bool unitemp_singlewire_sensorSetGPIO(Sensor* sensor, const GPIO* gpio);

/**
 * @brief Get sensor port
 * 
 * @param sensor Pointer to sensor
 * @return Pointer to GPIO
 */
const GPIO* unitemp_singlewire_sensorGetGPIO(Sensor* sensor);

/**
 * @brief Memory allocation for a sensor on a One Wire line
 * 
 * @param sensor Pointer to sensor
 * @param args Index on an array with the arguments of the parameters
 */
bool unitemp_singlewire_alloc(Sensor* sensor, char* args);

/**
 * @brief Freeing sensor instance memory
 * 
 * @param sensor Pointer to sensor
 */
bool unitemp_singlewire_free(Sensor* sensor);
#endif
