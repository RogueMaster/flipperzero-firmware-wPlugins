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

#include <furi_hal_i2c.h>

//I2C sensor structure
typedef struct I2CSensor {
    //Pointer to I2C interface
    const FuriHalI2cBusHandle* i2c;
    //Minimum device address on the I2C bus
    uint8_t minI2CAdr;
    //Maximum device address on the I2C bus
    uint8_t maxI2CAdr;
    //Current device address on the I2C bus
    uint8_t currentI2CAdr;
    //Pointer to its own sensor instance
    void* sensorInstance;
} I2CSensor;

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
bool unitemp_i2c_isDeviceReady(I2CSensor* i2c_sensor);

/**
 * @brief Memory allocation for sensor on I2C bus
 * @param sensor Pointer to sensor
 * @param st Sensor type
 * @return Istina if all ok
 */
bool unitemp_I2C_sensor_alloc(Sensor* sensor, char* args);

/**
 * @brief Freeing sensor instance memory
 * @param sensor Pointer to sensor
 */
bool unitemp_I2C_sensor_free(Sensor* sensor);

/**
 * @brief Update value from sensor
 * @param sensor Pointer to sensor
 * @return Update status
 */
UnitempStatus unitemp_I2C_sensor_update(Sensor* sensor);
/**
 * @brief Read the value of the reg register
 * @param i2c_sensor Pointer to sensor instance
 * @param reg Register number
 * @return Register value
 */
uint8_t unitemp_i2c_readReg(I2CSensor* i2c_sensor, uint8_t reg);

/**
 * @brief Read an array of values ​​from memory
 * @param i2c_sensor Pointer to sensor instance
 * @param startReg Register address from which reading will begin
 * @param len Number of bytes to read from the register
 * @param data Pointer to an array where the data will be read
 * @return True if the device returned data
 */
bool unitemp_i2c_readRegArray(I2CSensor* i2c_sensor, uint8_t startReg, uint8_t len, uint8_t* data);

/**
 * @brief Write value to register
 * @param i2c_sensor Pointer to sensor instance
 * @param reg Register number
 * @param value Value to write
 * @return True if the value is written
 */
bool unitemp_i2c_writeReg(I2CSensor* i2c_sensor, uint8_t reg, uint8_t value);

/**
 * @brief Write an array of values ​​to memory
 * @param i2c_sensor Pointer to sensor instance
 * @param startReg Register address from which recording will begin
 * @param len Number of bytes to read from the register
 * @param data Pointer to the array from which the data will be written
 * @return True if the device returned data
 */
bool unitemp_i2c_writeRegArray(I2CSensor* i2c_sensor, uint8_t startReg, uint8_t len, uint8_t* data);

/**
 * @brief Read data array over I2C bus
 * @param i2c_sensor Pointer to sensor instance
 * @param startReg Register address from which reading will begin
 * @param data Pointer to an array where the data will be read
 * @return True if the device returned data
 */
bool unitemp_i2c_readArray(I2CSensor* i2c_sensor, uint8_t len, uint8_t* data);

/**
 * @brief Write an array of data over the I2C bus
 * @param i2c_sensor Pointer to sensor instance
 * @param len Number of bytes to read from the register
 * @param data Pointer to the array from which the data will be written
 * @return True if the device returned data
 */
bool unitemp_i2c_writeArray(I2CSensor* i2c_sensor, uint8_t len, uint8_t* data);
#endif
