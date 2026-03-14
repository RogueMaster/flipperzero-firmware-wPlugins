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

#include <furi.h>
#include <furi_hal.h>
#include "spi_sensor.h"

const SensorConnectionInterface unitemp_spi = {
    .name = "SPI",
    .allocator = unitemp_spi_sensor_alloc,
    .mem_releaser = unitemp_spi_sensor_free,
    .updater = unitemp_spi_sensor_update};

static uint8_t sensors_count = 0;

bool unitemp_spi_sensor_alloc(Sensor* sensor, char* args) {
    if(args == NULL) return false;

    //Creating an SPI Sensor Instance
    SPISensor* instance = malloc(sizeof(SPISensor));
    if(instance == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s instance allocation error", sensor->name);
        return false;
    }
    sensor->instance = instance;

    //Definition GPIO chip select
    int gpio = 255;
    sscanf(args, "%d", &gpio);
    instance->cs_pin = unitemp_gpio_get_from_int(gpio);
    if(instance->cs_pin == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s GPIO setting error", sensor->name);
        free(instance);
        return false;
    }

    instance->spi = malloc(sizeof(FuriHalSpiBusHandle));
    memcpy(instance->spi, &furi_hal_spi_bus_handle_external, sizeof(FuriHalSpiBusHandle));

    instance->spi->cs = instance->cs_pin->pin;

    bool status = sensor->model->allocator(sensor, args);

    //Blocking GPIO ports
    sensors_count++;
    unitemp_gpio_lock(unitemp_gpio_get_from_int(2), &unitemp_spi);
    unitemp_gpio_lock(unitemp_gpio_get_from_int(3), &unitemp_spi);
    unitemp_gpio_lock(unitemp_gpio_get_from_int(5), &unitemp_spi);
    unitemp_gpio_lock(instance->cs_pin, &unitemp_spi);
    return status;
}

bool unitemp_spi_sensor_free(Sensor* sensor) {
    bool status = sensor->model->mem_releaser(sensor);
    unitemp_gpio_unlock(((SPISensor*)sensor->instance)->cs_pin);
    free(((SPISensor*)(sensor->instance))->spi);
    free(sensor->instance);

    if(--sensors_count == 0) {
        unitemp_gpio_unlock(unitemp_gpio_get_from_int(2));
        unitemp_gpio_unlock(unitemp_gpio_get_from_int(3));
        unitemp_gpio_unlock(unitemp_gpio_get_from_int(5));
    }

    return status;
}

bool unitemp_spi_sensor_init(Sensor* sensor) {
    return sensor->model->initializer(sensor);
}

bool unitemp_spi_sensor_deinit(Sensor* sensor) {
    UNUSED(sensor);

    return true;
}

SensorStatus unitemp_spi_sensor_update(Sensor* sensor) {
    return sensor->model->updater(sensor);
}
