#pragma once

#include "live_test.h"

// One test, three entries: the MPU-6050, MPU-6500 and MPU-9250 share an
// accelerometer register map byte for byte, so they share an implementation.
extern const LiveTest live_test_mpu6050;
extern const LiveTest live_test_mpu6500;
extern const LiveTest live_test_mpu9250;
