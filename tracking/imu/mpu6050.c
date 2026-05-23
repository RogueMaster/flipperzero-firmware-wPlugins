#include "imu.h"

#define MPU6050_TAG "MPU6050"
#define MPU6050_DEV_ADDR (0x68 << 1)

#define MPU6050_SMPLRT_DIV  0x19
#define MPU6050_CONFIG      0x1A
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_INT_ENABLE  0x38
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H  0x43
#define MPU6050_PWR_MGMT_1  0x6B
#define MPU6050_PWR_MGMT_2  0x6C
#define MPU6050_WHO_AM_I    0x75

bool mpu6050_begin() {
    FURI_LOG_I(MPU6050_TAG, "Init MPU6050");

    if(!furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, 50)) {
        FURI_LOG_E(MPU6050_TAG, "Not ready");
        return false;
    }

    uint8_t whoami = 0;
    furi_hal_i2c_read_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_WHO_AM_I, &whoami, 1, 50);
    if(whoami != 0x68) {
        FURI_LOG_E(MPU6050_TAG, "Unknown model: 0x%X", (int)whoami);
        return false;
    }

    uint8_t val = 0x80;
    furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_PWR_MGMT_1, &val, 1, 50);
    furi_delay_ms(100);

    val = 0x01;
    furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_PWR_MGMT_1, &val, 1, 50);
    furi_delay_ms(10);

    val = 0x00;
    furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_PWR_MGMT_2, &val, 1, 50);

    val = 0x03;
    furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_CONFIG, &val, 1, 50);

    val = 0x18;
    furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_GYRO_CONFIG, &val, 1, 50);

    val = 0x08;
    furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_ACCEL_CONFIG, &val, 1, 50);

    val = 0x01;
    furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_INT_ENABLE, &val, 1, 50);

    FURI_LOG_I(MPU6050_TAG, "Init OK");
    return true;
}

void mpu6050_end() {
    uint8_t val = 0x40;
    furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external, MPU6050_DEV_ADDR, MPU6050_PWR_MGMT_1, &val, 1, 50);
}

int mpu6050_read(double* vec) {
    uint8_t buf[14];
    if(!furi_hal_i2c_read_mem(
           &furi_hal_i2c_handle_external,
           MPU6050_DEV_ADDR,
           MPU6050_ACCEL_XOUT_H,
           buf,
           14,
           50)) {
        return 0;
    }

    int16_t ax = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ay = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az = (int16_t)((buf[4] << 8) | buf[5]);
    int16_t gx = (int16_t)((buf[8] << 8) | buf[9]);
    int16_t gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t gz = (int16_t)((buf[12] << 8) | buf[13]);

    vec[0] = (double)ax * 4 / 32768 * GRAVITY;
    vec[1] = (double)ay * 4 / 32768 * GRAVITY;
    vec[2] = (double)az * 4 / 32768 * GRAVITY;
    vec[3] = (double)gx * 2000 / 32768 * DEG_TO_RAD;
    vec[4] = (double)gy * 2000 / 32768 * DEG_TO_RAD;
    vec[5] = (double)gz * 2000 / 32768 * DEG_TO_RAD;

    return ACC_DATA_READY | GYR_DATA_READY;
}

struct imu_t imu_mpu6050 = {
    MPU6050_DEV_ADDR,
    mpu6050_begin,
    mpu6050_end,
    mpu6050_read,
    MPU6050_TAG,
};
