#include "rad_sens_i2c.h"
#include "rad_sens.h"

static bool
    rad_sens_read_register(uint8_t reg, uint8_t* data, size_t data_size, uint32_t timeout) {
    if(!furi_hal_i2c_tx(I2C_BUS, RAD_SENS_ADDRESS, &reg, 1, timeout)) {
        return false;
    }

    return furi_hal_i2c_rx(I2C_BUS, RAD_SENS_ADDRESS, data, data_size, timeout);
}

static bool rad_sens_write_register(uint8_t reg, uint8_t value, uint32_t timeout) {
    uint8_t buffer[2] = {reg, value};
    return furi_hal_i2c_tx(I2C_BUS, RAD_SENS_ADDRESS, buffer, sizeof(buffer), timeout);
}

bool rad_sens_read_data(RadSensModel* model) {
    furi_hal_i2c_acquire(I2C_BUS);

    uint32_t timeout = furi_ms_to_ticks(100);
    model->connected = false;
    model->verified = false;

    if(furi_hal_i2c_is_device_ready(I2C_BUS, RAD_SENS_ADDRESS, timeout) > 0) {
        model->connected = true;

        uint8_t buffer[4];
        uint8_t device_id = 0;

        if(rad_sens_read_register(RAD_SENS_ID_RG, buffer, 1, timeout)) {
            device_id = buffer[0];
        }

        if(device_id == RAD_SENS_ID) {
            model->verified = true;

            if(rad_sens_read_register(RAD_SENS_DYN_INTENSITY_RG, buffer, 3, timeout)) {
                model->dyn_intensity =
                    (((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) |
                     (uint32_t)buffer[2]);
            }

            if(rad_sens_read_register(RAD_SENS_STAT_INTENSITY_RG, buffer, 3, timeout)) {
                model->stat_intensity =
                    (((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) |
                     (uint32_t)buffer[2]);
            }

            if(rad_sens_read_register(RAD_SENS_SENSITIVITY_RG, buffer, 2, timeout)) {
                model->sensitivity = (((uint16_t)buffer[1] << 8) | (uint16_t)buffer[0]);
            }

            if(rad_sens_read_register(RAD_SENS_IMP_CNT_RG, buffer, 2, timeout)) {
                model->new_impulse_count = (((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
                model->impulse_count += model->new_impulse_count;
            }
        }
    }

    furi_hal_i2c_release(I2C_BUS);

    return model->verified;
}

bool rad_sens_set_sensitivity(uint16_t sensitivity) {
    bool success = false;
    uint8_t buffer[2];
    uint32_t timeout = furi_ms_to_ticks(100);

    furi_hal_i2c_acquire(I2C_BUS);

    if(furi_hal_i2c_is_device_ready(I2C_BUS, RAD_SENS_ADDRESS, timeout) > 0) {
        success = rad_sens_write_register(RAD_SENS_SENSITIVITY_RG, sensitivity & 0xFF, timeout);
        if(success) {
            furi_delay_ms(20);
            success = rad_sens_write_register(
                RAD_SENS_SENSITIVITY_RG + 1, (sensitivity >> 8) & 0xFF, timeout);
        }
        if(success) {
            furi_delay_ms(20);
            success = rad_sens_read_register(RAD_SENS_SENSITIVITY_RG, buffer, 2, timeout) &&
                      ((((uint16_t)buffer[1] << 8) | (uint16_t)buffer[0]) == sensitivity);
        }
    }

    furi_hal_i2c_release(I2C_BUS);

    return success;
}
