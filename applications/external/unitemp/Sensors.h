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
#ifndef UNITEMP_SENSORS_H_
#define UNITEMP_SENSORS_H_

#include <furi.h>
#include <furi_hal.h>

// Values returned when polling the sensor
typedef enum {
    UT_DATA_TYPE_TEMP,
    UT_DATA_TYPE_TEMP_HUM,
    UT_DATA_TYPE_TEMP_PRESS,
    UT_DATA_TYPE_TEMP_HUM_PRESS,
    UT_DATA_TYPE_TEMP_HUM_CO2,

    UT_DATA_TYPE_COUNT
} SensorDataType;

//Return Types
typedef enum {
    UT_SENSORSTATUS_OK, //Everything is fine, the polling is successful
    UT_SENSORSTATUS_TIMEOUT, //The sensor did not respond
    UT_SENSORSTATUS_EARLYPOOL, //Poll before the required delay
    UT_SENSORSTATUS_BADCRC, //Invalid checksum
    UT_SENSORSTATUS_ERROR, //Other errors
    UT_SENSORSTATUS_POLLING, //A transformation occurs in the sensor
    UT_SENSORSTATUS_INACTIVE, //The sensor is being edited or deleted
    UT_SENSORSTATUS_UNINITIALIZED,
    UT_SENSORSTATUS_INITIALIZED,
} SensorStatus;

typedef struct Sensor Sensor;

/**
 * @brief Function pointer to allocate memory and prepare a sensor instance
 */
typedef bool(SensorAllocator)(Sensor* sensor, char* args);
/**
 * @brief Pointer to the sensor memory release function
 */
typedef bool(SensorFree)(Sensor* sensor);
/**
 * @brief Sensor initialization function pointer
 */
typedef bool(SensorInitializer)(Sensor* sensor);
/**
 * @brief Pointer to the sensor deinitialization function
 */
typedef bool(SensorDeinitializer)(Sensor* sensor);
/**
 * @brief Pointer to the sensor value update function
 */
typedef SensorStatus(SensorUpdater)(Sensor* sensor);

//Sensor connection interface structure
typedef struct SensorConnectionInterface {
    //Interface name
    const char* name;
    //Interface memory allocation function
    SensorAllocator* allocator;
    //Interface memory release function
    SensorFree* mem_releaser;
    //Sensor value update function via interface
    SensorUpdater* updater;
} SensorConnectionInterface;

//Sensor types
typedef struct {
    //Sensor model
    const char* modelname;
    //Full name with analogues
    const char* altname;
    //Return type
    SensorDataType data_type;
    //Connection interface
    const SensorConnectionInterface* interface;
    //Sensor polling interval (ms)
    uint16_t polling_interval;
    //Sensor memory allocation function
    SensorAllocator* allocator;
    //Sensor memory release function
    SensorFree* mem_releaser;
    //Sensor initialization function
    SensorInitializer* initializer;
    //Sensor deinitialization function
    SensorDeinitializer* deinitializer;
    //Sensor value update function
    SensorUpdater* updater;
} SensorModel;

//Sensor
typedef struct Sensor {
    //Sensor user name
    char* name;
    //Temperature
    float temperature;
    //Relative humidity
    float humidity;
    //Atmospheric pressure
    float pressure;
    //CO2 concentration
    float co2;
    //Temperature offset (x10)
    int8_t temperature_offset;
    //Sensor type
    const SensorModel* model;
    //Last sensor poll status
    SensorStatus status;
    //Time of the last sensor poll
    uint32_t last_polling_time;
    //Sensor instance
    void* instance;
} Sensor;

/**
 * @brief Memory allocation for sensor
 * 
 * @param name Sensor name
 * @param type Sensor type
 * @param args Pointer to a string with sensor parameters
 * @return Pointer to the sensor in case of successful memory allocation, NULL on error
 */
Sensor* unitemp_sensor_alloc(char* name, const SensorModel* type, char* args);

/**
 * @brief Freeing up the memory of a specific sensor
 * @param sensor Pointer to sensor
 */
void unitemp_sensor_free(Sensor* sensor);

bool unitemp_sensor_init(Sensor* sensor);

bool unitemp_sensor_deinit(Sensor* sensor);

/**
 * @brief Add sensor to general list
 * @param sensor Pointer to sensor
 */
bool unitemp_sensors_add(Sensor* sensor);

/**
 * @brief Freeing up the memory of all sensors
 */
void unitemp_sensors_free(void);

/**
 * @brief Get number of loaded sensors
 * @return Number of sensors
 */
uint8_t unitemp_sensors_get_count(void);
/**
 * @brief Loading sensors from SD card
 * @return true if the upload was successful
 */
bool unitemp_sensors_load(void* ctx);

/**
 * @brief Saving sensors to SD card
 * @return True if save was successful
 */
bool unitemp_sensors_save(void* ctx);

/**
 * @brief Initializing loaded sensors
 * @param ctx Pointer to application context
 * @return true if everything went well
 */
bool unitemp_sensors_init(void* ctx);

/**
 * @brief Deinitialize loaded sensors
 * @return true if everything went well
 */
bool unitemp_sensors_deinit(void* ctx);

/**
 * @brief Update the data of the specified sensor
 * @param sensor Pointer to sensor
 * @param ctx Pointer to application context
 * @return Sensor poll status
 */
SensorStatus unitemp_sensor_update(Sensor* sensor, void* ctx);

/**
 * @brief Retrieves a sensor instance by its index.
 * 
 * @param index The index of the sensor to retrieve.
 * @return Pointer to the Sensor structure at the specified index, or NULL if index is out of bounds.
 */
Sensor* unitemp_sensors_get(uint8_t index);

/**
* @brief Get a list of available sensor types
* @return Pointer to a list of sensors
*/
const SensorModel** unitemp_sensors_models_get(void);
uint8_t unitemp_sensors_models_get_count(void);

void unitemp_sensors_reload(void* context);
bool unitemp_sensor_in_list(Sensor* sensor);
bool unitemp_sensor_delete(Sensor* sensor);
const SensorModel* unitemp_sensors_get_model_from_str(char* str);
int32_t unitemp_sensors_update_callback(void* ctx);

#endif // UNITEMP_SENSORS
