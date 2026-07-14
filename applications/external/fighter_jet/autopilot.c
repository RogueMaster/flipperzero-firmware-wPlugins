/* Copyright (c) 2025 Eero Prittinen */

#include <furi.h>
#include "autopilot.h"

turning_t autopilotFollower(double dist, double dir, double enemyDir, double ownDir) {
  UNUSED(dist);
  UNUSED(dir);

  if (cos(ownDir - enemyDir) > 0) {
    return TURNING_LEFT;
  } else if (sin(ownDir - enemyDir) > 0) {
    return TURNING_LEFT;
  } else {
    return TURNING_RIGHT;
  }
}
