#include "imu.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>

#include "driver/ICM42688P.h"

#define TAG "drop_imu"

static ICM42688P* s_imu = NULL;
static FuriHalSpiBusHandle* s_spi = NULL;

bool imu_open(void) {
    if(s_imu) return true;

    /* Clone the external SPI bus handle and point CS at the IMU module pins.
     * Same wiring as the FliPort collector (expo_app/drop_detect_app.c). */
    s_spi = malloc(sizeof(FuriHalSpiBusHandle));
    if(!s_spi) return false;
    memcpy(s_spi, &furi_hal_spi_bus_handle_external, sizeof(FuriHalSpiBusHandle));
    s_spi->cs = &gpio_ext_pc3;

    s_imu = icm42688p_alloc(s_spi, &gpio_ext_pb2);
    if(!s_imu) {
        FURI_LOG_E(TAG, "icm42688p_alloc failed");
        free(s_spi);
        s_spi = NULL;
        return false;
    }

    if(!icm42688p_init(s_imu)) {
        FURI_LOG_E(TAG, "icm42688p_init failed");
        icm42688p_free(s_imu);
        s_imu = NULL;
        free(s_spi);
        s_spi = NULL;
        return false;
    }

    if(!icm42688p_who_am_i_ok(s_imu)) FURI_LOG_W(TAG, "WHO_AM_I mismatch (continuing)");

    /* Match the training capture config exactly: 16 g / 2000 dps / 100 Hz. */
    icm42688p_enable_sensors(s_imu);
    icm42688p_accel_config(s_imu, AccelFullScale16G, DataRate100Hz);
    icm42688p_gyro_config(s_imu, GyroFullScale2000DPS, DataRate100Hz);

    FURI_LOG_I(TAG, "IMU open");
    return true;
}

bool imu_read_scaled(float out6[6]) {
    if(!s_imu) return false;

    ICM42688PRawData accel = {0};
    ICM42688PRawData gyro = {0};
    if(!icm42688p_read_accel_raw(s_imu, &accel)) return false;
    if(!icm42688p_read_gyro_raw(s_imu, &gyro)) return false;

    ICM42688PScaledData accel_s = {0};
    ICM42688PScaledData gyro_s = {0};
    icm42688p_apply_scale(&accel, icm42688p_accel_get_full_scale(s_imu), &accel_s);
    icm42688p_apply_scale(&gyro, icm42688p_gyro_get_full_scale(s_imu), &gyro_s);

    out6[0] = accel_s.x;
    out6[1] = accel_s.y;
    out6[2] = accel_s.z;
    out6[3] = gyro_s.x;
    out6[4] = gyro_s.y;
    out6[5] = gyro_s.z;
    return true;
}

void imu_close(void) {
    if(s_imu) {
        icm42688p_deinit(s_imu);
        icm42688p_free(s_imu);
        s_imu = NULL;
    }
    if(s_spi) {
        free(s_spi);
        s_spi = NULL;
    }
    FURI_LOG_I(TAG, "IMU closed");
}
