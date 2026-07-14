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
#ifndef UNITEMP_I2C
#define UNITEMP_I2C

#include "../unitemp.h"
#include "../sensors.h"

#include <furi_hal_i2c.h>

//I2C sensor structure
typedef struct I2CSensor {
    //Pointer to I2C interface
    const FuriHalI2cBusHandle* i2c_handle;
    //Minimum device address on the I2C bus
    uint8_t min_i2c_adress;
    //Maximum device address on the I2C bus
    uint8_t max_i2c_adress;
    //Current device address on the I2C bus
    uint8_t current_i2c_adress;
    //Pointer to its own sensor instance
    void* sensor_instance;
} I2CSensor;

extern const SensorConnectionInterface
    unitemp_i2c; //Proprietary single-wire protocol for DHTXX and AM23XX sensors

/**
 * @brief Lock the I2C bus
 * 
 * @param handle Pointer to bus
 */
void unitemp_i2c_acquire(const FuriHalI2cBusHandle* handle);

/**
 * @brief Check the presence of a sensor on the tire
 * 
 * @param i2c_sensor Pointer to sensor
 * @return True if the device has responded
 */
bool unitemp_i2c_is_device_ready(I2CSensor* i2c_sensor);

/**
 * @brief Memory allocation for sensor on I2C bus
 * @param sensor Pointer to sensor
 * @param st Sensor type
 * @return Istina if all ok
 */
bool unitemp_i2c_sensor_alloc(Sensor* sensor, char* args);

/**
 * @brief Freeing sensor instance memory
 * @param sensor Pointer to sensor
 */
bool unitemp_i2c_sensor_free(Sensor* sensor);

/**
 * @brief Update value from sensor
 * @param sensor Pointer to sensor
 * @return Update status
 */
SensorStatus unitemp_i2c_sensor_update(Sensor* sensor);
/**
 * @brief Read the value of the reg register
 * @param i2c_sensor Pointer to sensor instance
 * @param reg Register number
 * @return Register value
 */
uint8_t unitemp_i2c_read_reg(I2CSensor* i2c_sensor, uint8_t reg);

/**
 * @brief Read an array of values ​​from memory
 * @param i2c_sensor Pointer to sensor instance
 * @param startReg Register address from which reading will begin
 * @param len Number of bytes to read from the register
 * @param data Pointer to an array where the data will be read
 * @return True if the device returned data
 */
bool unitemp_i2c_read_reg_array(
    I2CSensor* i2c_sensor,
    uint8_t startReg,
    uint8_t len,
    uint8_t* data);

/**
 * @brief Write value to register
 * @param i2c_sensor Pointer to sensor instance
 * @param reg Register number
 * @param value Value to write
 * @return True if the value is written
 */
bool unitemp_i2c_write_reg(I2CSensor* i2c_sensor, uint8_t reg, uint8_t value);

/**
 * @brief Write an array of values ​​to memory
 * @param i2c_sensor Pointer to sensor instance
 * @param startReg Register address from which recording will begin
 * @param len Number of bytes to read from the register
 * @param data Pointer to the array from which the data will be written
 * @return True if the device returned data
 */
bool unitemp_i2c_write_reg_array(
    I2CSensor* i2c_sensor,
    uint8_t startReg,
    uint8_t len,
    uint8_t* data);

/**
 * @brief Read data array over I2C bus
 * @param i2c_sensor Pointer to sensor instance
 * @param startReg Register address from which reading will begin
 * @param data Pointer to an array where the data will be read
 * @return True if the device returned data
 */
bool unitemp_i2c_read_array(I2CSensor* i2c_sensor, uint8_t len, uint8_t* data);

/**
 * @brief Write an array of data over the I2C bus
 * @param i2c_sensor Pointer to sensor instance
 * @param len Number of bytes to read from the register
 * @param data Pointer to the array from which the data will be written
 * @return True if the device returned data
 */
bool unitemp_i2c_write_array(I2CSensor* i2c_sensor, uint8_t len, uint8_t* data);

/**
 * @brief Scans the I2C bus for connected devices within a sensor's address range.
 * 
 * This function performs a sequential scan of the I2C bus, checking device availability
 * at addresses within the range defined by the I2C sensor's minimum and maximum addresses.
 * It maintains the current scan position to allow for incremental scanning across multiple calls.
 * 
 * @param sensor Pointer to the Sensor structure containing the I2C sensor instance to scan.
 * 
 * @return The I2C address of the first detected device, or 0 if no device is found.
 *         
 * 
 */
uint8_t unitemp_i2c_bus_scan_next(I2CSensor* i2c_sensor);

/**
 * @brief Checks if the specified I2C address is already in use by a sensor.
 * 
 * @param addr The I2C address to check (8-bit format).
 * 
 * @return true if the address is already used by an active sensor, false otherwise.
 */
bool unitemp_i2c_addr_is_used(uint8_t addr);
#endif
