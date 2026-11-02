/*
 * imu.h — thin wrapper around the ICM42688P driver for the drop detector.
 * Owns one IMU instance on the external SPI bus and reads *scaled* samples
 * (accel in g, gyro in dps) at the same full-scale settings the trainer used,
 * so on-device features match training data.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate + init the IMU and configure it (16 g / 2000 dps / 100 Hz).
 * Returns false on any failure. Safe to call once; idempotent if already open. */
bool imu_open(void);

/* Read one scaled sample into out6 = { ax, ay, az, gx, gy, gz }.
 * Returns false if no IMU is open or a bus read failed. */
bool imu_read_scaled(float out6[6]);

/* Deinit + free the IMU and release the SPI handle. Safe to call when closed. */
void imu_close(void);

#ifdef __cplusplus
}
#endif
