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
#ifndef UNITEMP_GPIO_H_
#define UNITEMP_GPIO_H_
#include "../unitemp.h"
#include <furi.h>
#include <furi_hal.h>

//Unitemp GPIO Pin structure
typedef struct SensorGpioPin {
    const uint8_t num; //Pin number (2-7, 10, 12-17)
    const char* name; //Pin name from Flipper Zero shell
    const GpioPin* pin; //Pointer to GPIO pin structure from Furi HAL
} SensorGpioPin;

/**
 * @brief Converting the port number on the FZ case to SensorGpioPin
 * @param name Port number on the FZ case
 * @return Pointer to SensorGpioPin on success, NULL on error
 */
const SensorGpioPin* unitemp_gpio_get_from_int(uint8_t number);
/**
 * @brief Locking GPIO by specified interface
 * @param gpio Pointer to port
 * @param interface Pointer to the interface on which the port will be occupied
 */
void unitemp_gpio_lock(const SensorGpioPin* gpio, const SensorConnectionInterface* interface);

/**
 * @brief Unlocking the port
 * @param gpio Pointer to port
 */
void unitemp_gpio_unlock(const SensorGpioPin* gpio);

/**
 * @brief Get a pointer to the port available for the interface by index
 * @param interface Pointer to interface
 * @param index Port number (from 0 to unitemp_gpio_getAviablePortsCount())
 * @param extraport Pointer to an additional port that will be forced to be considered available. 
 * @return Pointer to an available port
 */
const SensorGpioPin* unitemp_gpio_get_aviable_pin(
    const SensorConnectionInterface* interface,
    uint8_t index,
    const SensorGpioPin* extraport);

/**
 * @brief Get the number of available ports for the specified interface
 * @param interface Pointer to interface
 * @return Number of available ports
 */
uint8_t unitemp_gpio_get_aviable_pin_count(
    const SensorConnectionInterface* interface,
    const SensorGpioPin* extraport);

const SensorGpioPin* unitemp_gpio_get_from_index(uint8_t index);

#endif //UNITEMP_GPIO_H_
