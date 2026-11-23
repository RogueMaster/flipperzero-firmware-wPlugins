#pragma once

#include <stdbool.h>

/*
 * Thin wrapper around the Flipper Zero VGM IMU so the game logic doesn't
 * depend on the sensor driver directly. The implementation uses the
 * flipperdevices/flipperzero-game-engine `sensors/imu.h` API.
 *
 * Coordinate convention (when Flipper is held "normally" — screen facing the
 * user, up-button pointing up):
 *   pitch:  0° = normal hold.  Positive = top tilts away (toward flat/screen-up).
 *           90° ≈ fully flat with D-pad facing up.
 *   roll:   0° = level.  Positive = right side tilts down (tilt right).
 */

typedef struct VgmTilt VgmTilt;

/* Allocate and start the IMU. Call once when entering a VGM-tilt game mode. */
VgmTilt* vgm_tilt_alloc(void);

/* Release IMU resources. */
void     vgm_tilt_free(VgmTilt* v);

/* Returns true if the VGM was detected and IMU data is valid. */
bool     vgm_tilt_present(const VgmTilt* v);

/* Current pitch in degrees (see convention above). */
float    vgm_tilt_pitch(VgmTilt* v);

/* Current roll in degrees (see convention above). */
float    vgm_tilt_roll(VgmTilt* v);
