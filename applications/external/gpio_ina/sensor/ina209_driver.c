/*
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <furi.h>
#include <furi_hal.h>

#include "../app_common.h"
#include "sensor_driver.h"
#include "ina209_driver.h"

#define INA209_I2C_TIMEOUT 2 // ms

#define INA209_REG_CONFIG 0x00
#define INA209_REG_STATUS 0x01
#define INA209_REG_SHUNT  0x03
#define INA209_REG_BUS    0x04
#define INA209_REG_POWER  0x05

#define INA209_REG_STATUS_CNVR 0x0020

typedef struct {
    SensorDriver base;
    // Sensor configuration
    Ina209Config config;
    // Set if the sensor is properly configured
    bool configured;
    // Last known sensor state
    SensorState state;

} Ina209Driver;

static void ina209_driver_free(SensorDriver* driver) {
    Ina209Driver* drv = (Ina209Driver*)driver;

    furi_check(drv != NULL);
    free(drv);
}

static bool ina209_write_reg(Ina209Driver* drv, uint8_t reg, uint16_t value) {
    furi_check(drv != NULL);

    return furi_hal_i2c_write_reg_16(
        &furi_hal_i2c_handle_external,
        drv->config.i2c_address << 1,
        reg,
        value,
        INA209_I2C_TIMEOUT);
}

static bool ina209_read_reg(Ina209Driver* drv, uint8_t reg, uint16_t* value) {
    furi_check(drv != NULL);

    return furi_hal_i2c_read_reg_16(
        &furi_hal_i2c_handle_external,
        drv->config.i2c_address << 1,
        reg,
        value,
        INA209_I2C_TIMEOUT);
}

static bool ina209_driver_tick(SensorDriver* driver) {
    Ina209Driver* drv = (Ina209Driver*)driver;
    SensorState prev_state = drv->state;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    drv->state.shunt_resistor = drv->config.shunt_resistor;
    drv->state.sensor_type = INA209_DRIVER_ID;

    if(!drv->configured) {
        // Configure INA209
        uint16_t config_reg = (1 << 13) | // 32V bus voltage range
                              (3 << 11) | // 320mv shunt voltage range
                              (15 << 7) | // BADC, 128 samples, 68.1ms conversion time
                              (15 << 3) | // SADC, 128 samples, 68.1ms conversion time
                              (7 << 0); // Shunt and bus, continuous

        if(ina209_write_reg(drv, INA209_REG_CONFIG, config_reg)) {
            FURI_LOG_I(TAG, "INA209 found at 0x%02X", drv->config.i2c_address);
            drv->configured = true;
        } else {
            FURI_LOG_I(TAG, "No I2C device found at 0x%02X", drv->config.i2c_address);
        }
    } else {
        uint16_t config_reg = 0;
        uint16_t bus_reg = 0;
        uint16_t shunt_reg = 0;
        uint16_t status_reg = 0;

        if(ina209_read_reg(drv, INA209_REG_STATUS, &status_reg) &&
           ina209_read_reg(drv, INA209_REG_SHUNT, &shunt_reg) &&
           ina209_read_reg(drv, INA209_REG_BUS, &bus_reg) &&
           ina209_read_reg(drv, INA209_REG_CONFIG, &config_reg)) {
            // Conversion done (CNVR bit is set) ?
            if((status_reg & INA209_REG_STATUS_CNVR) != 0) {
                // Calculate voltage on shunt resistor
                double shunt_voltage = 10E-6 * (int16_t)shunt_reg; // LSB = 10uV

                // Calculate current, bus voltage and power
                drv->state.current = shunt_voltage / drv->config.shunt_resistor;
                drv->state.voltage = 4E-3 * (bus_reg >> 3); // LSB = 4mV
                drv->state.ticks = furi_get_tick();
                drv->state.timestamp = furi_hal_rtc_get_timestamp();
                drv->state.ready = true;
            }
        } else {
            drv->state.ready = false;
            drv->configured = false;
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return memcmp(&prev_state, &drv->state, sizeof(SensorState)) != 0;
}

static void ina209_driver_get_state(SensorDriver* driver, SensorState* state) {
    Ina209Driver* drv = (Ina209Driver*)driver;

    furi_check(drv != NULL);
    furi_check(state != NULL);

    *state = drv->state;
}

SensorDriver* ina209_driver_alloc(const Ina209Config* config) {
    Ina209Driver* drv = malloc(sizeof(Ina209Driver));
    memset(drv, 0, sizeof(Ina209Driver));

    drv->base.free = ina209_driver_free;
    drv->base.get_state = ina209_driver_get_state;
    drv->base.tick = ina209_driver_tick;

    furi_check(config != NULL);

    drv->config = *((Ina209Config*)config);

    return &drv->base;
}
