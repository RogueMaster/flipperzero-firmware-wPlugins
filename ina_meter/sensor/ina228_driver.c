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
#include "ina228_driver.h"

#define INA228_I2C_TIMEOUT 2 // ms

#define INA228_REG_CONFIG     0x00
#define INA228_REG_ADC_CONFIG 0x01
#define INA228_REG_VSHUNT     0x04
#define INA228_REG_VBUS       0x05
#define INA228_REG_DIAG_ALRT  0x0B
#define INA228_REG_MANUF_ID   0x3E
#define INA228_REG_DEVICE_ID  0x3F

#define INA228_REG_CONFIG_ADCRANGE (1 << 4)
#define INA228_REG_DIAG_ALRT_CNVRF (1 << 1)

typedef struct {
    SensorDriver base;
    // Sensor configuration
    Ina228Config config;
    // Set if the sensor is properly configured
    bool configured;
    // Last known sensor state
    SensorState state;

} Ina228Driver;

static void ina228_driver_free(SensorDriver* driver) {
    Ina228Driver* drv = (Ina228Driver*)driver;

    furi_check(drv != NULL);
    free(drv);
}

static bool ina228_write_reg(Ina228Driver* drv, uint8_t reg, uint16_t value) {
    furi_check(drv != NULL);

    return furi_hal_i2c_write_reg_16(
        &furi_hal_i2c_handle_external,
        drv->config.i2c_address << 1,
        reg,
        value,
        INA228_I2C_TIMEOUT);
}

static bool ina228_read_reg(Ina228Driver* drv, uint8_t reg, uint16_t* value) {
    furi_check(drv != NULL);

    return furi_hal_i2c_read_reg_16(
        &furi_hal_i2c_handle_external,
        drv->config.i2c_address << 1,
        reg,
        value,
        INA228_I2C_TIMEOUT);
}

static bool ina228_read_reg24(Ina228Driver* drv, uint8_t reg, int32_t* value) {
    furi_check(drv != NULL);

    uint8_t bytes[3];

    if(!furi_hal_i2c_read_mem(
           &furi_hal_i2c_handle_external,
           drv->config.i2c_address << 1,
           reg,
           bytes,
           sizeof(bytes),
           INA228_I2C_TIMEOUT)) {
        return false;
    }

    *value = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];

    if(bytes[0] & 0x80) {
        *value |= 0xFF000000;
    }

    return true;
}

static bool ina228_driver_tick(SensorDriver* driver) {
    Ina228Driver* drv = (Ina228Driver*)driver;
    SensorState prev_state = drv->state;

    drv->state.shunt_resistor = drv->config.shunt_resistor;
    drv->state.sensor_type = INA228_DRIVER_ID;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    if(!drv->configured) {
        uint16_t manuf_id_reg = 0;
        uint16_t device_id_reg = 0;
        if(!ina228_read_reg(drv, INA228_REG_MANUF_ID, &manuf_id_reg) ||
           !ina228_read_reg(drv, INA228_REG_DEVICE_ID, &device_id_reg)) {
            FURI_LOG_I(TAG, "No I2C device found at 0x%02X", drv->config.i2c_address);
        } else if(manuf_id_reg != 0x5449 || device_id_reg != 0x2281) {
            FURI_LOG_I(TAG, "No INA228 found at 0x%02X", drv->config.i2c_address);
        } else {
            // Configure INA228
            uint16_t config_reg = (0 << 6) | // CONVDLY, no delay
                                  (0 << 5) | // TEMPCOMP, 0 => compensation disabled
                                  (drv->config.adc_range << 4); // ADCRANGE

            uint16_t adc_config_reg = (0x0B << 12) | // MODE, Continuous shunt and bus voltage
                                      (drv->config.vbus_conv_time << 9) | // VBUSCT
                                      (drv->config.vshunt_conv_time << 6) | // VSHCT
                                      (0 << 3) | // VTCT, 50us
                                      (drv->config.averaging << 0); // AVG

            if(!ina228_write_reg(drv, INA228_REG_CONFIG, config_reg) ||
               !ina228_write_reg(drv, INA228_REG_ADC_CONFIG, adc_config_reg)) {
                FURI_LOG_I(TAG, "Cannot cnofigure INA228 at 0x%02X", drv->config.i2c_address);
            } else {
                FURI_LOG_I(TAG, "INA228 found at 0x%02X and configured", drv->config.i2c_address);
                drv->configured = true;
            }
        }
    } else {
        uint16_t diag_alrt_reg = 0;
        int32_t vbus_reg = 0;
        int32_t vshunt_reg = 0;
        uint16_t config_reg = 0;

        if(ina228_read_reg(drv, INA228_REG_DIAG_ALRT, &diag_alrt_reg) &&
           ina228_read_reg24(drv, INA228_REG_VBUS, &vbus_reg) &&
           ina228_read_reg24(drv, INA228_REG_VSHUNT, &vshunt_reg) &&
           ina228_read_reg(drv, INA228_REG_CONFIG, &config_reg)) {
            // Conversion done (CNVR bit is set) ?
            if((diag_alrt_reg & INA228_REG_DIAG_ALRT_CNVRF) != 0) {
                // Calculate voltage on shunt resistor
                double shunt_voltage;
                if((config_reg & INA228_REG_CONFIG_ADCRANGE) == 0) {
                    // LSB = 312.5nV
                    shunt_voltage = 312.5E-9 * (vshunt_reg >> 4);
                } else {
                    // LSB = 78.125nV
                    shunt_voltage = 78.125E-9 * (vshunt_reg >> 4);
                }

                // Calculate current, bus voltage and power
                drv->state.current = shunt_voltage / drv->config.shunt_resistor;
                // LSB = 195.3125uV
                drv->state.voltage = 195.3125E-6 * (vbus_reg >> 4);

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

static void ina228_driver_get_state(SensorDriver* driver, SensorState* state) {
    Ina228Driver* drv = (Ina228Driver*)driver;

    furi_check(drv != NULL);
    furi_check(state != NULL);

    *state = drv->state;
}

SensorDriver* ina228_driver_alloc(const Ina228Config* config) {
    Ina228Driver* drv = malloc(sizeof(Ina228Driver));
    memset(drv, 0, sizeof(Ina228Driver));

    drv->base.free = ina228_driver_free;
    drv->base.get_state = ina228_driver_get_state;
    drv->base.tick = ina228_driver_tick;

    furi_check(config != NULL);

    drv->config = *((Ina228Config*)config);

    return &drv->base;
}
