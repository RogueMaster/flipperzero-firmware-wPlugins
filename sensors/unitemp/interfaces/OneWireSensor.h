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
#ifndef UNITEMP_OneWire
#define UNITEMP_OneWire

#include "../unitemp.h"

//Device family codes
typedef enum DallasFamilyCode {
    FC_DS18S20 = 0x10,
    FC_DS1822 = 0x22,
    FC_DS18B20 = 0x28,
} DallasFamilyCode;

//Sensor power mode
typedef enum PowerMode {
    PWR_PASSIVE, //Powered by data line
    PWR_ACTIVE //Powered by power supply
} PowerMode;

//One wire bus instance
typedef struct {
    //Sensor connection port
    const GPIO* gpio;
    //Number of devices on the bus
    //Updated when manually adding a sensor to this bus
    int8_t device_count;
    //Power supply mode for sensors on the bus
    PowerMode powerMode;
} OneWireBus;

//One wire sensor instance
typedef struct OneWireSensor {
    //Pointer to OneWire bus
    OneWireBus* bus;
    //Current address of the device on the OneWire bus
    uint8_t deviceID[8];
    //Device family code
    DallasFamilyCode familyCode;
} OneWireSensor;

/**
 * @brief Memory allocation for sensor on OneWire bus
 * @param sensor Pointer to sensor
 * @param args Pointer to an array of arguments with sensor parameters
 * @return Istina if all ok
 */
bool unitemp_onewire_sensor_alloc(Sensor* sensor, char* args);

/**
 * @brief Freeing sensor instance memory
 * @param sensor Pointer to sensor
 */
bool unitemp_onewire_sensor_free(Sensor* sensor);

/**
 * @brief Initializing the sensor on the one wire bus
 * @param sensor Pointer to sensor
 * @return True if initialization is successful
 */
bool unitemp_onewire_sensor_init(Sensor* sensor);

/**
 * @brief Deinitializing the sensor
 * @param sensor Pointer to sensor
 */
bool unitemp_onewire_sensor_deinit(Sensor* sensor);

/**
 * @brief Update value from sensor
 * @param sensor Pointer to sensor
 * @return Update status
 */
UnitempStatus unitemp_onewire_sensor_update(Sensor* sensor);

/**
 * @brief Allocation of memory for the one wire bus and its initialization
 * @param gpio Port on which to create a bus
 * @return If successful, returns a pointer to the one wire bus
 */
OneWireBus* unitemp_onewire_bus_alloc(const GPIO* gpio);

/**
 * @brief One wire bus initialization
 * 
 * @param bus Pointer to bus
 * @return True if initialization is successful
 */
bool unitemp_onewire_bus_init(OneWireBus* bus);

/**
 * @brief One wire bus deinitialization
 * 
 * @param bus Pointer to bus
 * @return True if the bus has been deinitialized, false if there are still devices on the bus
 */
bool unitemp_onewire_bus_deinit(OneWireBus* bus);

/**
 * @brief Starting communication with sensors on the one wire bus
 * @param bus Pointer to bus
 * @return True if at least one device has responded
 */
bool unitemp_onewire_bus_start(OneWireBus* bus);

/**
 * @brief Send 1 bit of data to one wire bus
 * @param bus Pointer to bus
 * @param state Logical level
 */
void unitemp_onewire_bus_send_bit(OneWireBus* bus, bool state);

/**
 * @brief Writing a byte to the one wire bus
 * 
 * @param bus Pointer to one wire bus
 * @param data Byte to write
 */
void unitemp_onewire_bus_send_byte(OneWireBus* bus, uint8_t data);

/**
 * @brief Writing a byte array to the one wire bus
 * 
 * @param bus Pointer to one wire bus
 * @param data Pointer to the array from which the data will be written
 * @param len Number of bytes
 */
void unitemp_onewire_bus_send_byteArray(OneWireBus* bus, uint8_t* data, uint8_t len);

/**
 * @brief Reading a bit on a one wire bus
 * 
 * @param bus Pointer to one wire bus
 * @return Logical bit level
 */
bool unitemp_onewire_bus_read_bit(OneWireBus* bus);

/**
 * @brief Reading a byte from the One Wire bus
 * 
 * @param bus Pointer to one wire bus
 * @return Byte of information
 */
uint8_t unitemp_onewire_bus_read_byte(OneWireBus* bus);

/**
 * @brief Reading a byte array from the One Wire bus
 * 
 * @param bus Pointer to one wire bus
 * @param data Pointer to the array where the data will be written
 * @param len Number of bytes
 */
void unitemp_onewire_bus_read_byteArray(OneWireBus* bus, uint8_t* data, uint8_t len);

/**
 * @brief Check the checksum of a data array
 * 
 * @param data Pointer to a data array
 * @param len Array length (including CRC byte)
 * @return True if the checksum is correct
 */
bool unitemp_onewire_CRC_check(uint8_t* data, uint8_t len);

/**
 * @brief Get the model name of the sensor on the One Wire bus
 * 
 * @param sensor Pointer to sensor
 * @return Pointer to the string with the title
 */
char* unitemp_onewire_sensor_getModel(Sensor* sensor);

/**
 * @brief Reading the identifier of a single sensor. 
 * 
 * @param instance Pointer to the sensor instance
 * @return True if the code was successfully read, false if there is no device or there is more than one device on the bus
 */
bool unitemp_oneWire_sensor_readID(OneWireSensor* instance);

/**
 * @brief Command to select a specific sensor by its ID
 * @param instance Pointer to one wire sensor
 */
void unitemp_onewire_bus_select_sensor(OneWireSensor* instance);

/**
 * @brief Initializing the process of searching for addresses on the one wire bus
 */
void unitemp_onewire_bus_enum_init(void);

/**
 * @brief Enumerates devices on the one wire bus and gets the next address
 * @param bus Pointer to one wire bus
 * @return Returns a pointer to a buffer containing an eight-byte address value, or NULL if the search is completed
 */
uint8_t* unitemp_onewire_bus_enum_next(OneWireBus* bus);

/**
 * @brief Compare sensor IDs
 * 
 * @param id1 Pointer to the address of the first sensor
 * @param id2 Pointer to the address of the second sensor
 * @return True if IDs are identical
 */
bool unitemp_onewire_id_compare(uint8_t* id1, uint8_t* id2);

extern const SensorType Dallas;
#endif
