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
#include "i2c_sensor.h"
#include "../helpers/unitemp_gpio.h"

const SensorConnectionInterface unitemp_i2c = {
    .name = "I2C",
    .allocator = unitemp_i2c_sensor_alloc,
    .mem_releaser = unitemp_i2c_sensor_free,
    .updater = unitemp_i2c_sensor_update};

static uint8_t sensors_count = 0;

void unitemp_i2c_acquire(const FuriHalI2cBusHandle* handle) {
    furi_hal_i2c_acquire(handle);
    LL_GPIO_SetPinPull(gpio_ext_pc1.port, gpio_ext_pc1.pin, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(gpio_ext_pc0.port, gpio_ext_pc0.pin, LL_GPIO_PULL_UP);
}

bool unitemp_i2c_is_device_ready(I2CSensor* i2c_sensor) {
    unitemp_i2c_acquire(i2c_sensor->i2c_handle);
    bool status =
        furi_hal_i2c_is_device_ready(i2c_sensor->i2c_handle, i2c_sensor->current_i2c_adress, 10);
    furi_hal_i2c_release(i2c_sensor->i2c_handle);
    return status;
}

uint8_t unitemp_i2c_read_reg(I2CSensor* i2c_sensor, uint8_t reg) {
    //Bus lock
    unitemp_i2c_acquire(i2c_sensor->i2c_handle);
    uint8_t buff[1] = {0};

    furi_hal_i2c_read_mem(
        i2c_sensor->i2c_handle, i2c_sensor->current_i2c_adress, reg, buff, 1, 10);
    furi_hal_i2c_release(i2c_sensor->i2c_handle);
    return buff[0];
}

bool unitemp_i2c_read_array(I2CSensor* i2c_sensor, uint8_t len, uint8_t* data) {
    unitemp_i2c_acquire(i2c_sensor->i2c_handle);
    bool status =
        furi_hal_i2c_rx(i2c_sensor->i2c_handle, i2c_sensor->current_i2c_adress, data, len, 10);
    furi_hal_i2c_release(i2c_sensor->i2c_handle);
    return status;
}

bool unitemp_i2c_read_reg_array(
    I2CSensor* i2c_sensor,
    uint8_t startReg,
    uint8_t len,
    uint8_t* data) {
    unitemp_i2c_acquire(i2c_sensor->i2c_handle);
    bool status = furi_hal_i2c_read_mem(
        i2c_sensor->i2c_handle, i2c_sensor->current_i2c_adress, startReg, data, len, 10);
    furi_hal_i2c_release(i2c_sensor->i2c_handle);
    return status;
}

bool unitemp_i2c_write_reg(I2CSensor* i2c_sensor, uint8_t reg, uint8_t value) {
    //Bus lock
    unitemp_i2c_acquire(i2c_sensor->i2c_handle);
    uint8_t buff[1] = {value};
    bool status = furi_hal_i2c_write_mem(
        i2c_sensor->i2c_handle, i2c_sensor->current_i2c_adress, reg, buff, 1, 10);
    furi_hal_i2c_release(i2c_sensor->i2c_handle);
    return status;
}

bool unitemp_i2c_write_array(I2CSensor* i2c_sensor, uint8_t len, uint8_t* data) {
    unitemp_i2c_acquire(i2c_sensor->i2c_handle);
    bool status =
        furi_hal_i2c_tx(i2c_sensor->i2c_handle, i2c_sensor->current_i2c_adress, data, len, 10);
    furi_hal_i2c_release(i2c_sensor->i2c_handle);
    return status;
}

bool unitemp_i2c_write_reg_array(
    I2CSensor* i2c_sensor,
    uint8_t startReg,
    uint8_t len,
    uint8_t* data) {
    //Bus lock
    unitemp_i2c_acquire(i2c_sensor->i2c_handle);
    bool status = furi_hal_i2c_write_mem(
        i2c_sensor->i2c_handle, i2c_sensor->current_i2c_adress, startReg, data, len, 10);
    furi_hal_i2c_release(i2c_sensor->i2c_handle);
    return status;
}

bool unitemp_i2c_sensor_alloc(Sensor* sensor, char* args) {
    bool status = false;
    I2CSensor* instance = malloc(sizeof(I2CSensor));
    if(instance == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s instance allocation error", sensor->name);
        return false;
    }
    instance->i2c_handle = &furi_hal_i2c_handle_external;
    sensor->instance = instance;

    //Specifying the functions of initialization, deinitialization and data update, as well as the address on the I2C bus
    status = sensor->model->allocator(sensor, args);
    int i2c_addr;
    sscanf(args, "%X", &i2c_addr);

    //Setting the I2C bus address
    if(i2c_addr >= instance->min_i2c_adress && i2c_addr <= instance->max_i2c_adress) {
        instance->current_i2c_adress = i2c_addr;
    } else {
        instance->current_i2c_adress = instance->min_i2c_adress;
    }

    //Blocking GPIO ports
    sensors_count++;
    unitemp_gpio_lock(unitemp_gpio_get_from_int(15), &unitemp_i2c);
    unitemp_gpio_lock(unitemp_gpio_get_from_int(16), &unitemp_i2c);

    return status;
}

bool unitemp_i2c_sensor_free(Sensor* sensor) {
    bool status = sensor->model->mem_releaser(sensor);
    free(sensor->instance);
    if(--sensors_count == 0) {
        unitemp_gpio_unlock(unitemp_gpio_get_from_int(15));
        unitemp_gpio_unlock(unitemp_gpio_get_from_int(16));
    }

    return status;
}

SensorStatus unitemp_i2c_sensor_update(Sensor* sensor) {
    return sensor->model->updater(sensor);
}

bool unitemp_i2c_addr_is_used(uint8_t addr) {
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        Sensor* sensor = unitemp_sensors_get(i);
        if(sensor->model->interface == &unitemp_i2c) {
            I2CSensor* i2c_sensor = sensor->instance;
            if(i2c_sensor->current_i2c_adress == addr) return true;
        }
    }

    return false;
}

uint8_t unitemp_i2c_bus_scan_next(I2CSensor* i2c_sensor) {
    static I2CSensor* last_sensor;
    static uint8_t last_addr = 0;

    if(i2c_sensor != last_sensor) {
        last_sensor = i2c_sensor;
        last_addr = i2c_sensor->current_i2c_adress;
    }

    for(uint8_t i = 0; i < (i2c_sensor->max_i2c_adress - i2c_sensor->min_i2c_adress + 1); i++) {
        last_addr += 2;
        if(last_addr > i2c_sensor->max_i2c_adress || last_addr < i2c_sensor->min_i2c_adress) {
            last_addr = i2c_sensor->min_i2c_adress;
        }

        unitemp_i2c_acquire(i2c_sensor->i2c_handle);
        bool result = furi_hal_i2c_is_device_ready(i2c_sensor->i2c_handle, last_addr, 10);
        furi_hal_i2c_release(i2c_sensor->i2c_handle);

        bool is_used = unitemp_i2c_addr_is_used(last_addr);
        UNITEMP_DEBUG(
            "Address 0x%02X is %s and %s used for other sensor",
            (last_addr >> 1),
            result ? "found" : "not found",
            is_used ? "is" : "isn't");

        if(result && !is_used) {
            return last_addr;
        }
    }

    return 0;

    // static I2CSensor* last_sensor;
    // static uint8_t last_addr = 0;

    // if(i2c_sensor != last_sensor) {
    //     last_sensor = i2c_sensor;
    //     last_addr = i2c_sensor->current_i2c_adress;
    // }

    // uint8_t start_addr = last_addr;
    // for(uint8_t i = 2; last_addr + i != start_addr; i += 2) {
    //     if(last_addr + i > i2c_sensor->max_i2c_adress ||
    //        last_addr + i < i2c_sensor->min_i2c_adress) {
    //         last_addr = i2c_sensor->min_i2c_adress;
    //         i = 0;
    //         if(last_addr == start_addr) return 0;
    //     }

    //     unitemp_i2c_acquire(i2c_sensor->i2c_handle);
    //     bool result = furi_hal_i2c_is_device_ready(i2c_sensor->i2c_handle, last_addr + i, 10);
    //     furi_hal_i2c_release(i2c_sensor->i2c_handle);

    //     UNITEMP_DEBUG("Address 0x%02X is %s", (last_addr + i), result ? "found" : "not found");

    //     if(result) {
    //         last_addr += i;
    //         return last_addr;
    //     }
    // }
    // return 0;
}
