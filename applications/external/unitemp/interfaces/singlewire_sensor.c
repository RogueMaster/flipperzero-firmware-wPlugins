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

#include "singlewire_sensor.h"

#include "../sensors/DHTxx.h"
#include "../sensors/AM2320.h"

// Maximum number of polling ticks while waiting for the sensor
#define POLLING_TIMEOUT_TICKS 5000

const SensorConnectionInterface unitemp_singlewire = {
    .name = "Single wire",
    .allocator = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .updater = unitemp_singlewire_update};

bool unitemp_singlewire_alloc(Sensor* sensor, char* args) {
    if(sensor == NULL || args == NULL) return false;

    SingleWireSensor* instance = malloc(sizeof(SingleWireSensor));
    if(instance == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s instance allocation error", sensor->name);
        return false;
    }
    sensor->instance = instance;

    int gpio = 255;
    sscanf(args, "%d", &gpio);

    if(unitemp_singlewire_sensor_gpio_set(sensor, unitemp_gpio_get_from_int(gpio))) {
        UNITEMP_DEBUG("Sensor %s GPIO set to %d", sensor->name, gpio);
        return true;
    }

    FURI_LOG_E(APP_NAME, "Sensor %s GPIO setting error", sensor->name);
    free(instance);
    return false;
}

bool unitemp_singlewire_free(Sensor* sensor) {
    SingleWireSensor* instance = sensor->instance;
    unitemp_gpio_unlock(instance->data_pin);
    free(instance);

    return true;
}

bool unitemp_singlewire_sensor_gpio_set(Sensor* sensor, const SensorGpioPin* data_pin) {
    if(sensor == NULL || data_pin == NULL) return false;

    SingleWireSensor* instance = sensor->instance;
    if(instance->data_pin != NULL) unitemp_gpio_unlock(instance->data_pin);
    instance->data_pin = data_pin;
    unitemp_gpio_lock(instance->data_pin, &unitemp_singlewire);
    return true;
}

const SensorGpioPin* unitemp_singlewire_sensor_gpio_get(Sensor* sensor) {
    if(sensor == NULL) return NULL;
    SingleWireSensor* instance = sensor->instance;
    return instance->data_pin;
}

bool unitemp_singlewire_init(Sensor* sensor) {
    if(sensor == NULL) return false;

    SingleWireSensor* instance = ((Sensor*)sensor)->instance;
    if(instance == NULL || instance->data_pin == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor pointer is null!");
        return false;
    }
    // High level by default
    furi_hal_gpio_write(instance->data_pin->pin, true);
    // Operation mode - OpenDrain, pull-up enabled just in case
    furi_hal_gpio_init(
        instance->data_pin->pin, // FZ port
        GpioModeOutputOpenDrain, // Operation mode - open drain
        GpioPullUp, // Force pull-up of the data line to power
        GpioSpeedVeryHigh); // Operating speed - maximum
    sensor->last_polling_time = furi_get_tick();
    return (unitemp_singlewire_update(sensor) == UT_SENSORSTATUS_OK);
}

bool unitemp_singlewire_deinit(Sensor* sensor) {
    if(sensor == NULL) return false;

    SingleWireSensor* instance = ((Sensor*)sensor)->instance;
    if(instance == NULL || instance->data_pin == NULL) return false;
    // Low level by default
    furi_hal_gpio_write(instance->data_pin->pin, false);
    // Mode - analog, pull-up disabled
    furi_hal_gpio_init(
        instance->data_pin->pin, // FZ port
        GpioModeAnalog, // Operation mode - analog
        GpioPullNo, // Pull-up disabled
        GpioSpeedLow); // Operating speed - minimum
    return true;
}

SensorStatus unitemp_singlewire_update(Sensor* sensor) {
    if(sensor == NULL) return UT_SENSORSTATUS_ERROR;
    SingleWireSensor* instance = sensor->instance;

    // Array for receiving data
    uint8_t data[5] = {0};

    /* Request */
    // Pull the line low
    furi_hal_gpio_write(instance->data_pin->pin, false);
    // Wait more than 18 ms
    furi_delay_ms(19);
    // Disable interrupts to ensure accurate timing
    FURI_CRITICAL_ENTER();
    // Raise the line
    furi_hal_gpio_write(instance->data_pin->pin, true);

    /* Sensor response */
    // Counter variable
    uint16_t timeout = 0;

    // Wait for the line to go high
    while(!furi_hal_gpio_read(instance->data_pin->pin)) {
        timeout++;
        if(timeout > POLLING_TIMEOUT_TICKS) {
            // Enable interrupts
            FURI_CRITICAL_EXIT();
            // Return the indicator of a missing sensor
            return UT_SENSORSTATUS_TIMEOUT;
        }
    }
    timeout = 0;

    // Wait for the line to go low
    while(furi_hal_gpio_read(instance->data_pin->pin)) {
        timeout++;
        if(timeout > POLLING_TIMEOUT_TICKS) {
            // Enable interrupts
            FURI_CRITICAL_EXIT();
            // Return the indicator of a missing sensor
            return UT_SENSORSTATUS_TIMEOUT;
        }
    }

    // Wait for the line to go high
    while(!furi_hal_gpio_read(instance->data_pin->pin)) {
        timeout++;
        if(timeout > POLLING_TIMEOUT_TICKS) {
            // Enable interrupts
            FURI_CRITICAL_EXIT();
            // Return the indicator of a missing sensor
            return UT_SENSORSTATUS_TIMEOUT;
        }
    }
    timeout = 0;

    // Wait for the line to go low
    while(furi_hal_gpio_read(instance->data_pin->pin)) {
        timeout++;
        if(timeout > POLLING_TIMEOUT_TICKS) {
            // Enable interrupts
            FURI_CRITICAL_EXIT();
            // Return the indicator of a missing sensor
            return UT_SENSORSTATUS_TIMEOUT;
        }
    }

    /* Reading data from the sensor */
    // Receive 5 bytes
    for(uint8_t a = 0; a < 5; a++) {
        for(uint8_t b = 7; b != 255; b--) {
            uint16_t hT = 0, lT = 0;
            // While the line is low, increment the lT variable
            while(!furi_hal_gpio_read(instance->data_pin->pin) && lT != 65535)
                lT++;
            // While the line is high, increment the hT variable
            while(furi_hal_gpio_read(instance->data_pin->pin) && hT != 65535)
                hT++;
            // If hT is greater than lT, a one was received
            if(hT > lT) data[a] |= (1 << b);
        }
    }
    // Enable interrupts
    FURI_CRITICAL_EXIT();

    // Check the checksum
    if((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) {
        // If the checksum does not match, return an error
        return UT_SENSORSTATUS_BADCRC;
    }

    /* Convert data to explicit form */
    // DHT11 and DHT12
    if(sensor->model == &DHT11) {
        sensor->humidity = (float)data[0];
        sensor->temperature = (float)data[2];

        // Check if the temperature is negative
        if(data[3] != 0) {
            // Check the sign
            if(!(data[3] & (1 << 7))) {
                // Add the positive fractional part
                sensor->temperature += data[3] * 0.1f;
            } else {
                // Here we make the value negative
                data[3] &= ~(1 << 7);
                sensor->temperature += data[3] * 0.1f;
                sensor->temperature *= -1;
            }
        }
    }

    // DHT21, DHT22, AM2320
    if(sensor->model == &DHT21 || sensor->model == &DHT22 || sensor->model == &AM2320_SW) {
        sensor->humidity = (float)(((uint16_t)data[0] << 8) | data[1]) / 10;

        uint16_t raw = (((uint16_t)data[2] << 8) | data[3]);
        // Check if the temperature is negative
        if(READ_BIT(raw, 1 << 15)) {
            // Check the data encoding method
            if(READ_BIT(raw, 0x6000)) {
                // Not original
                sensor->temperature = (float)((int16_t)raw) / 10;
            } else {
                // Original sensor
                CLEAR_BIT(raw, 1 << 15);
                sensor->temperature = (float)(raw) / -10;
            }
        } else {
            sensor->temperature = (float)(raw) / 10;
        }
    }
    // Return the successful poll indicator
    return UT_SENSORSTATUS_OK;
}
