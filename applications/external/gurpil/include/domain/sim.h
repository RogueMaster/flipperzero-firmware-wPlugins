#pragma once

#include "include/domain/shapes.h"

#include <stdint.h>

/*
 * Pure, deterministic "stable-hybrid" speed/progress model (no furi, no I/O, no floats).
 *
 * The vehicle always rolls forward; the mounted wheel SHAPE and the TERRAIN kind at the
 * vehicle's current position tune how fast it advances (see domain/shapes.h). This is not a
 * physics engine: sim_step looks up shape_speed_factor for the terrain under the vehicle,
 * eases the current speed toward that target, and advances distance by the eased speed.
 *
 * Fixed-point scale: values are represented as `real_value << SIM_FP_SHIFT`, i.e. Q(SIM_FP_SHIFT)
 * fixed-point. `SIM_FP_ONE` is the fixed-point representation of the integer value 1, handy for
 * callers/tests comparing against "about one game-x-unit".
 */

enum {
    SIM_FP_SHIFT = 8,               // fractional bits of the fixed-point scale.
    SIM_FP_ONE = 1 << SIM_FP_SHIFT, // fixed-point representation of the integer value 1.

    // The fastest speed_fp a run can ever reach: shape_speed_factor's own maximum (FACTOR_BEST
    // == 256, i.e. 1.0x) applied to sim.c's base speed. sim.c's BASE_SPEED_FP aliases this same
    // value (single source of truth) since a caller outside sim.c (application/game.c's
    // game_speed_permille) needs it too, to scale the current speed onto a 0..1000 HUD gauge.
    SIM_MAX_SPEED_FP = 24 << SIM_FP_SHIFT,
};

typedef struct {
    int32_t distance_fp; // total distance travelled so far, in fixed-point game-x units.
    int32_t speed_fp;    // current speed, in fixed-point game-x units per second.
    int16_t vehicle_y;   // terrain height (game units, TERRAIN_HEIGHT_MIN..MAX) under the vehicle.
} SimState;

/* Resets `state` to a run's start: zero distance, zero speed, vehicle_y at the start terrain
 * height. No seed parameter is needed: the opening terrain zone is always flat (terrain.c),
 * and a flat zone's height is seed-independent, so x=0's height is the same for every seed. */
void sim_init(SimState *state);

/* Advances `state` by one fixed timestep of `dt_ms` milliseconds:
 *   1. samples the terrain kind at the vehicle's current x (distance_fp >> SIM_FP_SHIFT).
 *   2. looks up shape_speed_factor(shape, kind) and derives the target speed from it.
 *   3. eases speed_fp one step toward that target (a smooth ramp, not an instant jump).
 *   4. advances distance_fp by the eased speed over dt_ms (monotonic non-decreasing).
 *   5. updates vehicle_y to the terrain height at the new distance.
 * Deterministic: replaying the same (initial state, shape, seed, dt_ms) sequence always
 * produces the same resulting states. */
void sim_step(SimState *state, ShapeId shape, uint32_t seed, uint32_t dt_ms);
