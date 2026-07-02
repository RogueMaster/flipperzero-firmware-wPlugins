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
#include "ina219_driver.h"

#define INA219_I2C_TIMEOUT 2 // ms

#define INA219_REG_CONFIG 0x00
#define INA219_REG_SHUNT  0x01
#define INA219_REG_BUS    0x02
#define INA219_REG_POWER  0x03

#define INA219_REG_BUS_CNVR 0x0002

typedef struct {
    SensorDriver base;
    // Sensor configuration
    Ina219Config config;
    // Set if the sensor is properly configured
    bool configured;
    // Last known sensor state
    SensorState state;

} Ina219Driver;

static void ina219_driver_free(SensorDriver* driver) {
    Ina219Driver* drv = (Ina219Driver*)driver;

    furi_check(drv != NULL);
    free(drv);
}

static bool ina219_write_reg(Ina219Driver* drv, uint8_t reg, uint16_t value) {
    furi_check(drv != NULL);

    return furi_hal_i2c_write_reg_16(
        &furi_hal_i2c_handle_external,
        drv->config.i2c_address << 1,
        reg,
        value,
        INA219_I2C_TIMEOUT);
}

static bool ina219_read_reg(Ina219Driver* drv, uint8_t reg, uint16_t* value) {
    furi_check(drv != NULL);

    return furi_hal_i2c_read_reg_16(
        &furi_hal_i2c_handle_external,
        drv->config.i2c_address << 1,
        reg,
        value,
        INA219_I2C_TIMEOUT);
}

static bool ina219_driver_tick(SensorDriver* driver) {
    Ina219Driver* drv = (Ina219Driver*)driver;
    SensorState prev_state = drv->state;

    drv->state.shunt_resistor = drv->config.shunt_resistor;
    drv->state.sensor_type = INA219_DRIVER_ID;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    if(!drv->configured) {
        // Configure INA219
        uint16_t config_reg = (1 << 13) | // 32V bus voltage range
                              (3 << 11) | // 320mv shunt voltage range
                              (15 << 7) | // BADC, 128 samples, 68.1ms conversion time
                              (15 << 3) | // SADC, 128 samples, 68.1ms conversion time
                              (7 << 0); // Shunt and bus, continuous

        if(ina219_write_reg(drv, INA219_REG_CONFIG, config_reg)) {
            FURI_LOG_I(TAG, "INA219 found at 0x%02X", drv->config.i2c_address);
            drv->configured = true;
        } else {
            FURI_LOG_I(TAG, "No I2C device found at 0x%02X", drv->config.i2c_address);
        }
    } else {
        uint16_t config_reg = 0;
        uint16_t bus_reg = 0;
        uint16_t shunt_reg = 0;

        if(ina219_read_reg(drv, INA219_REG_BUS, &bus_reg) &&
           ina219_read_reg(drv, INA219_REG_SHUNT, &shunt_reg) &&
           ina219_read_reg(drv, INA219_REG_CONFIG, &config_reg)) {
            // Conversion done (CNVR bit is set) ?
            if((bus_reg & INA219_REG_BUS_CNVR) != 0) {
                // Calculate voltage on shunt resistor
                double shunt_voltage = 10E-6 * (int16_t)shunt_reg; // LSB = 10uV

                // Calculate current, bus voltage and power
                drv->state.current = shunt_voltage / drv->config.shunt_resistor;
                drv->state.voltage = 4E-3 * (bus_reg >> 3); // LSB = 4mV

                // Clear INA219_REG_BUS_CNVR bit by reading the power register
                uint16_t power_reg;
                ina219_read_reg(drv, INA219_REG_POWER, &power_reg);

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

static void ina219_driver_get_state(SensorDriver* driver, SensorState* state) {
    Ina219Driver* drv = (Ina219Driver*)driver;

    furi_check(drv != NULL);
    furi_check(state != NULL);

    *state = drv->state;
}

SensorDriver* ina219_driver_alloc(const Ina219Config* config) {
    Ina219Driver* drv = malloc(sizeof(Ina219Driver));
    memset(drv, 0, sizeof(Ina219Driver));

    drv->base.free = ina219_driver_free;
    drv->base.get_state = ina219_driver_get_state;
    drv->base.tick = ina219_driver_tick;

    furi_check(config != NULL);

    drv->config = *((Ina219Config*)config);

    return &drv->base;
}
