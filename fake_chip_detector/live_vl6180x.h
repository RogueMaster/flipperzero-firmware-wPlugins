#pragma once

#include "live_test.h"

// ST VL6180X — time-of-flight laser rangefinder.
//
// The live test runs real single-shot measurements and shows the distance in
// millimetres. Waving a hand at the sensor moves the number, which is the
// whole point: a board relabelled from a cheaper proximity part can copy the
// 0xB4 model ID byte, but it cannot time a photon.
extern const LiveTest live_test_vl6180x;
