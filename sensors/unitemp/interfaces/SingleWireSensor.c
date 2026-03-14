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
#include "SingleWireSensor.h"

// Maximum number of polling ticks while waiting for the sensor
#define POLLING_TIMEOUT_TICKS 500

/* Sensor types and their parameters */
const SensorType DHT11 = {
    .typename = "DHT11",
    .interface = &SINGLE_WIRE,
    .datatype = UT_DATA_TYPE_TEMP_HUM,
    .pollingInterval = 2000,
    .allocator = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .initializer = unitemp_singlewire_init,
    .deinitializer = unitemp_singlewire_deinit,
    .updater = unitemp_singlewire_update};
const SensorType DHT12_SW = {
    .typename = "DHT12",
    .interface = &SINGLE_WIRE,
    .datatype = UT_DATA_TYPE_TEMP_HUM,
    .pollingInterval = 2000,
    .allocator = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .initializer = unitemp_singlewire_init,
    .deinitializer = unitemp_singlewire_deinit,
    .updater = unitemp_singlewire_update};
const SensorType DHT21 = {
    .typename = "DHT21",
    .altname = "DHT21/AM2301",
    .interface = &SINGLE_WIRE,
    .datatype = UT_DATA_TYPE_TEMP_HUM,
    .pollingInterval = 1000,
    .allocator = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .initializer = unitemp_singlewire_init,
    .deinitializer = unitemp_singlewire_deinit,
    .updater = unitemp_singlewire_update};
const SensorType DHT22 = {
    .typename = "DHT22",
    .altname = "DHT22/AM2302",
    .interface = &SINGLE_WIRE,
    .datatype = UT_DATA_TYPE_TEMP_HUM,
    .pollingInterval = 2000,
    .allocator = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .initializer = unitemp_singlewire_init,
    .deinitializer = unitemp_singlewire_deinit,
    .updater = unitemp_singlewire_update};
const SensorType AM2320_SW = {
    .typename = "AM2320",
    .altname = "AM2320 (single wire)",
    .interface = &SINGLE_WIRE,
    .datatype = UT_DATA_TYPE_TEMP_HUM,
    .pollingInterval = 2000,
    .allocator = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .initializer = unitemp_singlewire_init,
    .deinitializer = unitemp_singlewire_deinit,
    .updater = unitemp_singlewire_update};

bool unitemp_singlewire_alloc(Sensor* sensor, char* args) {
    if(args == NULL) return false;
    SingleWireSensor* instance = malloc(sizeof(SingleWireSensor));
    if(instance == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s instance allocation error", sensor->name);
        return false;
    }
    sensor->instance = instance;

    int gpio = 255;
    sscanf(args, "%d", &gpio);

    if(unitemp_singlewire_sensorSetGPIO(sensor, unitemp_gpio_getFromInt(gpio))) {
        return true;
    }
    FURI_LOG_E(APP_NAME, "Sensor %s GPIO setting error", sensor->name);
    free(instance);
    return false;
}
bool unitemp_singlewire_free(Sensor* sensor) {
    free(sensor->instance);

    return true;
}

bool unitemp_singlewire_init(Sensor* sensor) {
    SingleWireSensor* instance = ((Sensor*)sensor)->instance;
    if(instance == NULL || instance->gpio == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor pointer is null!");
        return false;
    }
    unitemp_gpio_lock(instance->gpio, &SINGLE_WIRE);
    // High level by default
    furi_hal_gpio_write(instance->gpio->pin, true);
    // Operation mode - OpenDrain, pull-up enabled just in case
    furi_hal_gpio_init(
        instance->gpio->pin, // FZ port
        GpioModeOutputOpenDrain, // Operation mode - open drain
        GpioPullUp, // Force pull-up of the data line to power
        GpioSpeedVeryHigh); // Operating speed - maximum
    return true;
}

bool unitemp_singlewire_deinit(Sensor* sensor) {
    SingleWireSensor* instance = ((Sensor*)sensor)->instance;
    if(instance == NULL || instance->gpio == NULL) return false;
    unitemp_gpio_unlock(instance->gpio);
    // Low level by default
    furi_hal_gpio_write(instance->gpio->pin, false);
    // Mode - analog, pull-up disabled
    furi_hal_gpio_init(
        instance->gpio->pin, // FZ port
        GpioModeAnalog, // Operation mode - analog
        GpioPullNo, // Pull-up disabled
        GpioSpeedLow); // Operating speed - minimum
    return true;
}

bool unitemp_singlewire_sensorSetGPIO(Sensor* sensor, const GPIO* gpio) {
    if(sensor == NULL || gpio == NULL) return false;
    SingleWireSensor* instance = sensor->instance;
    instance->gpio = gpio;
    return true;
}
const GPIO* unitemp_singlewire_sensorGetGPIO(Sensor* sensor) {
    if(sensor == NULL) return NULL;
    SingleWireSensor* instance = sensor->instance;
    return instance->gpio;
}

UnitempStatus unitemp_singlewire_update(Sensor* sensor) {
    SingleWireSensor* instance = sensor->instance;

    // Array for receiving data
    uint8_t data[5] = {0};

    /* Request */
    // Pull the line low
    furi_hal_gpio_write(instance->gpio->pin, false);
    // Wait more than 18 ms
    furi_delay_ms(19);
    // Disable interrupts so nothing interferes with processing the data
    __disable_irq();
    // Raise the line
    furi_hal_gpio_write(instance->gpio->pin, true);

    /* Sensor response */
    // Counter variable
    uint16_t timeout = 0;

    // Wait for the line to go high
    while(!furi_hal_gpio_read(instance->gpio->pin)) {
        timeout++;
        if(timeout > POLLING_TIMEOUT_TICKS) {
            // Enable interrupts
            __enable_irq();
            // Return the indicator of a missing sensor
            return UT_SENSORSTATUS_TIMEOUT;
        }
    }
    timeout = 0;

    // Wait for the line to go low
    while(furi_hal_gpio_read(instance->gpio->pin)) {
        timeout++;
        if(timeout > POLLING_TIMEOUT_TICKS) {
            // Enable interrupts
            __enable_irq();
            // Return the indicator of a missing sensor
            return UT_SENSORSTATUS_TIMEOUT;
        }
    }

    // Wait for the line to go high
    while(!furi_hal_gpio_read(instance->gpio->pin)) {
        timeout++;
        if(timeout > POLLING_TIMEOUT_TICKS) {
            // Enable interrupts
            __enable_irq();
            // Return the indicator of a missing sensor
            return UT_SENSORSTATUS_TIMEOUT;
        }
    }
    timeout = 0;

    // Wait for the line to go low
    while(furi_hal_gpio_read(instance->gpio->pin)) {
        timeout++;
        if(timeout > POLLING_TIMEOUT_TICKS) {
            // Enable interrupts
            __enable_irq();
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
            while(!furi_hal_gpio_read(instance->gpio->pin) && lT != 65535)
                lT++;
            // While the line is high, increment the hT variable
            while(furi_hal_gpio_read(instance->gpio->pin) && hT != 65535)
                hT++;
            // If hT is greater than lT, a one was received
            if(hT > lT) data[a] |= (1 << b);
        }
    }
    // Enable interrupts
    __enable_irq();

    // Check the checksum
    if((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) {
        // If the checksum does not match, return an error
        return UT_SENSORSTATUS_BADCRC;
    }

    /* Convert data to explicit form */
    // DHT11 and DHT12
    if(sensor->type == &DHT11 || sensor->type == &DHT12_SW) {
        sensor->hum = (float)data[0];
        sensor->temp = (float)data[2];

        // Check if the temperature is negative
        if(data[3] != 0) {
            // Check the sign
            if(!(data[3] & (1 << 7))) {
                // Add the positive fractional part
                sensor->temp += data[3] * 0.1f;
            } else {
                // Here we make the value negative
                data[3] &= ~(1 << 7);
                sensor->temp += data[3] * 0.1f;
                sensor->temp *= -1;
            }
        }
    }

    // DHT21, DHT22, AM2320
    if(sensor->type == &DHT21 || sensor->type == &DHT22 || sensor->type == &AM2320_SW) {
        sensor->hum = (float)(((uint16_t)data[0] << 8) | data[1]) / 10;

        uint16_t raw = (((uint16_t)data[2] << 8) | data[3]);
        // Check if the temperature is negative
        if(READ_BIT(raw, 1 << 15)) {
            // Check the data encoding method
            if(READ_BIT(raw, 0x6000)) {
                // Not original
                sensor->temp = (float)((int16_t)raw) / 10;
            } else {
                // Original sensor
                CLEAR_BIT(raw, 1 << 15);
                sensor->temp = (float)(raw) / -10;
            }
        } else {
            sensor->temp = (float)(raw) / 10;
        }
    }
    // Return the successful poll indicator
    return UT_SENSORSTATUS_OK;
}
