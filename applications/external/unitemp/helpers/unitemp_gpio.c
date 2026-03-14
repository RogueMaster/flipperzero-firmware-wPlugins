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
#include "unitemp_gpio.h"
#include "./interfaces/i2c_sensor.h"
#include "./interfaces/onewire_sensor.h"
#include "./interfaces/singlewire_sensor.h"
#include "./interfaces/spi_sensor.h"

//List of available GPIO pins with their numbers and names
#define SENSOR_PINS_COUNT (int)(sizeof(gpio_list) / sizeof(const SensorGpioPin))
static const SensorGpioPin gpio_list[] = {
    {2, "2 (A7)", &gpio_ext_pa7},
    {3, "3 (A6)", &gpio_ext_pa6},
    {4, "4 (A4)", &gpio_ext_pa4},
    {5, "5 (B3)", &gpio_ext_pb3},
    {6, "6 (B2)", &gpio_ext_pb2},
    {7, "7 (C3)", &gpio_ext_pc3},
    {10, "10 (SWC)", &gpio_swclk},
    {12, "12 (SIO)", &gpio_swdio},
    {13, "13 (TX)", &gpio_usart_tx},
    {14, "14 (RX)", &gpio_usart_rx},
    {15, "15 (C1)", &gpio_ext_pc1},
    {16, "16 (C0)", &gpio_ext_pc0},
    {17, "17 (1W)", &gpio_ibutton}};

// List of interfaces that are attached to GPIO(defined by index)
//NULL - port is free, pointer to interface - port is occupied by this interface
static const SensorConnectionInterface* gpio_interfaces_list[SENSOR_PINS_COUNT] = {0};

const SensorGpioPin* unitemp_gpio_get_from_int(uint8_t number) {
    for(uint8_t i = 0; i < SENSOR_PINS_COUNT; i++) {
        if(gpio_list[i].num == number) {
            return &gpio_list[i];
        }
    }
    return NULL;
}

const SensorGpioPin* unitemp_gpio_get_from_index(uint8_t index) {
    return &gpio_list[index];
}

uint8_t unitemp_gpio_to_index(const GpioPin* gpio) {
    if(gpio == NULL) return 255;

    for(uint8_t i = 0; i < SENSOR_PINS_COUNT; i++) {
        if(gpio_list[i].pin->pin == gpio->pin && gpio_list[i].pin->port == gpio->port) {
            return i;
        }
    }

    return 255;
}

void unitemp_gpio_lock(const SensorGpioPin* gpio, const SensorConnectionInterface* interface) {
    furi_check(gpio);
    furi_check(interface);

    uint8_t i = unitemp_gpio_to_index(gpio->pin);
    if(i == 255) return;
    gpio_interfaces_list[i] = interface;
    UNITEMP_DEBUG("%s has been locked for interface %s", gpio->name, interface->name);
}

void unitemp_gpio_unlock(const SensorGpioPin* gpio) {
    furi_check(gpio);

    uint8_t i = unitemp_gpio_to_index(gpio->pin);
    if(i == 255) return;
    gpio_interfaces_list[i] = NULL;
    UNITEMP_DEBUG("%s has been unlocked", gpio->name);
}

const SensorGpioPin* unitemp_gpio_get_aviable_pin(
    const SensorConnectionInterface* interface,
    uint8_t index,
    const SensorGpioPin* extraport) {
    //Check for I2C
    if(interface == &unitemp_i2c) {
        if((gpio_interfaces_list[10] == NULL || gpio_interfaces_list[10] == &unitemp_i2c) &&
           (gpio_interfaces_list[11] == NULL || gpio_interfaces_list[11] == &unitemp_i2c)) {
            //Return some true
            return unitemp_gpio_get_from_index(0);
        } else {
            return NULL;
        }
    }
    if(interface == &unitemp_spi) {
        if(!((gpio_interfaces_list[0] == NULL || gpio_interfaces_list[0] == &unitemp_spi) &&
             (gpio_interfaces_list[1] == NULL || gpio_interfaces_list[1] == &unitemp_spi) &&
             (gpio_interfaces_list[3] == NULL || gpio_interfaces_list[3] == &unitemp_spi))) {
            return NULL;
        }
    }

    uint8_t aviable_index = 0;
    for(uint8_t i = 0; i < SENSOR_PINS_COUNT; i++) {
        //Check for one wire
        if(interface == &unitemp_1w) {
            if(((gpio_interfaces_list[i] == NULL || gpio_interfaces_list[i] == &unitemp_1w)) ||
               (unitemp_gpio_get_from_index(i) == extraport)) {
                if(aviable_index == index) {
                    return unitemp_gpio_get_from_index(i);
                } else {
                    aviable_index++;
                }
            }
        }
        //Check for single wire dataline
        if(interface == &unitemp_singlewire) {
            if(gpio_interfaces_list[i] == NULL || unitemp_gpio_get_from_index(i) == extraport) {
                if(aviable_index == index) {
                    return unitemp_gpio_get_from_index(i);
                } else {
                    aviable_index++;
                }
            }
        }
        //Check for SPI CS
        if(interface == &unitemp_spi) {
            if((gpio_interfaces_list[i] == NULL || unitemp_gpio_get_from_index(i) == extraport) &&
               i != 0 && i != 1 && i != 3) {
                if(aviable_index == index) {
                    return unitemp_gpio_get_from_index(i);
                } else {
                    aviable_index++;
                }
            }
        }
    }

    return NULL;
}

uint8_t unitemp_gpio_get_aviable_pin_count(
    const SensorConnectionInterface* interface,
    const SensorGpioPin* extraport) {
    uint8_t aviable_ports_count = 0;
    for(uint8_t i = 0; i < SENSOR_PINS_COUNT; i++) {
        //Check for one wire
        if(interface == &unitemp_1w) {
            if(((gpio_interfaces_list[i] == NULL || gpio_interfaces_list[i] == &unitemp_1w)) ||
               (unitemp_gpio_get_from_index(i) == extraport)) {
                aviable_ports_count++;
            }
        }

        //Check for single wire
        if(interface == &unitemp_singlewire || interface == &unitemp_spi) {
            if(gpio_interfaces_list[i] == NULL || (unitemp_gpio_get_from_index(i) == extraport)) {
                UNITEMP_DEBUG("%s pin is aviable", unitemp_gpio_get_from_index(i)->name);
                aviable_ports_count++;
            }
        }

        if(interface == &unitemp_i2c) {
            if((gpio_interfaces_list[10] == NULL || gpio_interfaces_list[10] == &unitemp_i2c) &&
               (gpio_interfaces_list[11] == NULL || gpio_interfaces_list[11] == &unitemp_i2c)) {
                return 2;
            } else {
                return 0;
            }
        }
    }
    return aviable_ports_count;
}
