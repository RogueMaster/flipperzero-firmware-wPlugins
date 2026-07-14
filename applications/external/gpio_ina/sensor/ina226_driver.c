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
#include "ina226_driver.h"

#define INA226_I2C_TIMEOUT 2 // ms

#define INA226_REG_CONFIG   0x00
#define INA226_REG_SHUNT    0x01
#define INA226_REG_BUS      0x02
#define INA226_REG_POWER    0x03
#define INA226_REG_MASK     0x06
#define INA226_REG_MANUF_ID 0xFE
#define INA226_REG_DIE_ID   0xFF

#define INA226_REG_MASK_CVRF 0x0008

typedef struct {
    SensorDriver base;
    // Sensor configuration
    Ina226Config config;
    // Set if the sensor is properly configured
    bool configured;
    // Last known sensor state
    SensorState state;

} Ina226Driver;

static void ina226_driver_free(SensorDriver* driver) {
    Ina226Driver* drv = (Ina226Driver*)driver;

    furi_check(drv != NULL);
    free(drv);
}

static bool ina226_write_reg(Ina226Driver* drv, uint8_t reg, uint16_t value) {
    furi_check(drv != NULL);

    return furi_hal_i2c_write_reg_16(
        &furi_hal_i2c_handle_external,
        drv->config.i2c_address << 1,
        reg,
        value,
        INA226_I2C_TIMEOUT);
}

static bool ina226_read_reg(Ina226Driver* drv, uint8_t reg, uint16_t* value) {
    furi_check(drv != NULL);

    return furi_hal_i2c_read_reg_16(
        &furi_hal_i2c_handle_external,
        drv->config.i2c_address << 1,
        reg,
        value,
        INA226_I2C_TIMEOUT);
}

static bool ina226_driver_tick(SensorDriver* driver) {
    Ina226Driver* drv = (Ina226Driver*)driver;
    SensorState prev_state = drv->state;

    drv->state.shunt_resistor = drv->config.shunt_resistor;
    drv->state.sensor_type = INA226_DRIVER_ID;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    if(!drv->configured) {
        uint16_t manuf_id_reg = 0;
        uint16_t die_id_reg = 0;
        if(!ina226_read_reg(drv, INA226_REG_MANUF_ID, &manuf_id_reg) ||
           !ina226_read_reg(drv, INA226_REG_DIE_ID, &die_id_reg)) {
            FURI_LOG_I(TAG, "No I2C device found at 0x%02X", drv->config.i2c_address);
        } else if(manuf_id_reg != 0x5449 || die_id_reg != 0x2260) {
            FURI_LOG_I(TAG, "No INA226 found at 0x%02X", drv->config.i2c_address);
        } else {
            // Configure INA226
            uint16_t config_reg = (drv->config.averaging << 9) | // AVG
                                  (drv->config.vbus_conv_time << 6) | // VBUSCT
                                  (drv->config.vshunt_conv_time << 3) | // VSHCT
                                  (7 << 0); // Shunt and bus, continuous

            if(!ina226_write_reg(drv, INA226_REG_CONFIG, config_reg)) {
                FURI_LOG_I(TAG, "Cannot configure INA226 0x%02X", drv->config.i2c_address);
            } else {
                FURI_LOG_I(TAG, "INA226 found at 0x%02X and configured", drv->config.i2c_address);
                drv->configured = true;
            }
        }
    } else {
        uint16_t mask_reg = 0;
        uint16_t config_reg = 0;
        uint16_t bus_reg = 0;
        uint16_t shunt_reg = 0;

        if(ina226_read_reg(drv, INA226_REG_MASK, &mask_reg) &&
           ina226_read_reg(drv, INA226_REG_BUS, &bus_reg) &&
           ina226_read_reg(drv, INA226_REG_SHUNT, &shunt_reg) &&
           ina226_read_reg(drv, INA226_REG_CONFIG, &config_reg)) {
            // Conversion done (CNVR bit is set) ?
            if((mask_reg & INA226_REG_MASK_CVRF) != 0) {
                // Calculate voltage on shunt resistor
                double shunt_voltage = 2.5E-6 * (int16_t)shunt_reg; // LSB = 2.5uV

                // Calculate current, bus voltage and power
                drv->state.current = shunt_voltage / drv->config.shunt_resistor;
                drv->state.voltage = 1.25E-3 * bus_reg; // LSB = 1.25mV

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

static void ina226_driver_get_state(SensorDriver* driver, SensorState* state) {
    Ina226Driver* drv = (Ina226Driver*)driver;

    furi_check(drv != NULL);
    furi_check(state != NULL);

    *state = drv->state;
}

SensorDriver* ina226_driver_alloc(const Ina226Config* config) {
    Ina226Driver* drv = malloc(sizeof(Ina226Driver));
    memset(drv, 0, sizeof(Ina226Driver));

    drv->base.free = ina226_driver_free;
    drv->base.get_state = ina226_driver_get_state;
    drv->base.tick = ina226_driver_tick;

    furi_check(config != NULL);

    drv->config = *((Ina226Config*)config);

    return &drv->base;
}
