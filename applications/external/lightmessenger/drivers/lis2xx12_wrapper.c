#include "lis2xx12_wrapper.h"
#include "lis2dh12_reg.h"
// static const char* TAG = "LIS2DH12W";
#define RW_TIMEOUT 1000

int32_t lis2dh12_init(void* stmdev) {
    // Interrupt registers
    lis2dh12_int1_cfg_t int1_cfg = {0};
    lis2dh12_int2_cfg_t int2_cfg = {0};

    // Pin registers
    lis2dh12_ctrl_reg3_t pin_int1_cfg = {0};
    lis2dh12_ctrl_reg6_t pin_int2_cfg = {0};

    pin_int2_cfg.int_polarity = PARAM_INT_POLARITY;
    pin_int2_cfg.i2_ia1 = PROPERTY_DISABLE;
    pin_int2_cfg.i2_ia2 = PROPERTY_ENABLE;
    pin_int2_cfg.i2_boot = PROPERTY_DISABLE;
    pin_int2_cfg.i2_act = PROPERTY_DISABLE;
    pin_int2_cfg.i2_click = PROPERTY_DISABLE;
    pin_int2_cfg.not_used_01 = PROPERTY_DISABLE;
    pin_int2_cfg.not_used_02 = PROPERTY_DISABLE;
    lis2dh12_pin_int2_config_set(stmdev, &pin_int2_cfg);

    pin_int1_cfg.i1_zyxda = PROPERTY_DISABLE;
    pin_int1_cfg.i1_ia1 = PROPERTY_ENABLE;
    pin_int1_cfg.i1_ia2 = PROPERTY_DISABLE;
    pin_int1_cfg.i1_click = PROPERTY_DISABLE;
    pin_int1_cfg.i1_overrun = PROPERTY_DISABLE;
    pin_int1_cfg.i1_wtm = PROPERTY_DISABLE;
    pin_int1_cfg.not_used_01 = PROPERTY_DISABLE;
    pin_int1_cfg.not_used_02 = PROPERTY_DISABLE;
    lis2dh12_pin_int1_config_set(stmdev, &pin_int1_cfg);

    // lis2dh12 general configuration
    lis2dh12_high_pass_bandwidth_set(stmdev, PARAM_ACCELEROMETER_HIGH_PASS);
    lis2dh12_data_rate_set(stmdev, PARAM_OUTPUT_DATA_RATE);
    lis2dh12_full_scale_set(stmdev, PARAM_ACCELEROMETER_SCALE);
    lis2dh12_operating_mode_set(stmdev, PARAM_OPERATION_MODE);

    // lis2dh12 INT1
    int1_cfg.zhie = PROPERTY_ENABLE; // Z High interrupt enable on INT1
    int1_cfg.zlie = PROPERTY_DISABLE; // Z Low interrupt disable on INT1
    int1_cfg.aoi = PROPERTY_ENABLE;
    int1_cfg._6d = PROPERTY_ENABLE;
    lis2dh12_int1_gen_conf_set(stmdev, &int1_cfg);

    lis2dh12_int1_pin_notification_mode_set(stmdev, LIS2DH12_INT1_PULSED);
    lis2dh12_int1_gen_threshold_set(
        stmdev, PARAM_INT1_THRESHOLD); // default threshold, overwritten
    lis2dh12_int1_gen_duration_set(stmdev, PARAM_INT1_DURATION);

    // lis2dh12 INT2
    int2_cfg.zhie = PROPERTY_DISABLE; // Z High interrupt disable on INT1
    int2_cfg.zlie = PROPERTY_ENABLE; // Z Low interrupt enable on INT1
    int2_cfg.aoi = PROPERTY_ENABLE;
    int2_cfg._6d = PROPERTY_ENABLE;
    lis2dh12_int2_gen_conf_set(stmdev, &int2_cfg);

    lis2dh12_int2_pin_notification_mode_set(stmdev, LIS2DH12_INT2_PULSED);
    lis2dh12_int2_gen_threshold_set(
        stmdev, PARAM_INT2_THRESHOLD); // default threshold, overwritten
    lis2dh12_int2_gen_duration_set(stmdev, PARAM_INT2_DURATION);

    return 0;
}

void lis2dh12_set_sensitivity(void* stmdev, uint8_t sensitivity) {
    lis2dh12_int1_gen_threshold_set(stmdev, sensitivity);
    lis2dh12_int2_gen_threshold_set(stmdev, sensitivity);
}

int32_t platform_write(void* handle, uint8_t reg, const uint8_t* bufp, uint16_t len) {
    UNUSED(handle);
    furi_hal_i2c_acquire(I2C_BUS);
    furi_hal_i2c_write_mem(I2C_BUS, LIS2DH12_I2C_ADD_L, reg, bufp, len, RW_TIMEOUT);
    furi_hal_i2c_release(I2C_BUS);
    return 0;
}

int32_t platform_read(void* handle, uint8_t reg, uint8_t* bufp, uint16_t len) {
    UNUSED(handle);
    furi_hal_i2c_acquire(I2C_BUS);
    reg |= 0x80;
    furi_hal_i2c_trx(I2C_BUS, LIS2DH12_I2C_ADD_L, &reg, 1, bufp, len, RW_TIMEOUT);
    furi_hal_i2c_release(I2C_BUS);
    return 0;
}
