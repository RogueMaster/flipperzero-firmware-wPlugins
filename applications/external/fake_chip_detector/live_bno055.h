#pragma once

#include "live_test.h"

// Bosch BNO055 — 9-axis IMU with on-chip sensor fusion.
//
// The live test puts the part into NDOF mode and reads the fused Euler
// heading plus the magnetometer calibration level. A relabelled MPU-class die
// cannot do this: NDOF fusion and the CALIB_STAT register are the BNO055's
// own firmware, not a register value that can be copied.
extern const LiveTest live_test_bno055;
