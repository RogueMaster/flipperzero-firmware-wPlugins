#include "include/domain/sim.h"

#include "include/domain/speed_ramp.h"
#include "include/domain/terrain.h"

enum {
    // Base speed at shape_speed_factor's full value (256 == 1.0x), in fixed-point game-x units
    // per second. Every shape/terrain combination scales this down (or, in principle, up) via
    // the factor looked up below. Aliases SIM_MAX_SPEED_FP (sim.h) — the single source of truth
    // for "the fastest speed_fp a run can ever reach" — since factor 256 is also
    // shape_speed_factor's own maximum.
    BASE_SPEED_FP = SIM_MAX_SPEED_FP,

    // shape_speed_factor's fixed-point scale (256 == 1.0x); dividing by this applies the factor.
    SHAPE_FACTOR_SCALE = 256,

    // Per-tick easing: each step closes 1/SPEED_EASE_DIVISOR of the remaining gap between the
    // current and target speed, so speed ramps smoothly across ticks instead of snapping.
    SPEED_EASE_DIVISOR = 4,

    // Milliseconds per second: scales a millisecond timestep onto the per-second speed unit.
    MS_PER_SECOND = 1000,
};

// Moves `current` one step toward `target`, closing 1/`divisor` of the remaining gap. Uses
// ceiling division on the gap's magnitude so a nonzero gap always shrinks by at least 1 unit
// per call (plain truncating division can stall forever just short of the target) while never
// stepping past `target` (the step magnitude never exceeds the gap magnitude).
static int32_t ease_toward(int32_t current, int32_t target, int32_t divisor) {
    int32_t diff = target - current;
    if (diff == 0) {
        return current;
    }
    int32_t magnitude = diff > 0 ? diff : -diff;
    int32_t step_magnitude = (magnitude + divisor - 1) / divisor;
    return current + (diff > 0 ? step_magnitude : -step_magnitude);
}

void sim_init(SimState *state) {
    state->distance_fp = 0;
    state->speed_fp = 0;
    // Seed value is irrelevant here: the opening zone is always flat, and a flat zone's height
    // is seed-independent (see terrain.c), so this is the correct x=0 height for every seed.
    state->vehicle_y = terrain_at(0, 0).height;
}

void sim_step(SimState *state, ShapeId shape, uint32_t seed, uint32_t dt_ms) {
    int32_t current_x = state->distance_fp >> SIM_FP_SHIFT;
    TerrainSample sample = terrain_at(seed, current_x);

    // Fold the distance-driven ramp stage into the target's single division: no precision lost,
    // and the product stays well inside int32 (max ~= 256 * (24<<8) * 3 ~= 4.7M).
    uint8_t stage = speed_ramp_stage(current_x);
    uint16_t factor = shape_speed_factor(shape, sample.kind);
    int32_t target_speed_fp =
        ((int32_t)factor * BASE_SPEED_FP * stage) / (SHAPE_FACTOR_SCALE * SPEED_RAMP_MAX_STAGE);

    state->speed_fp = ease_toward(state->speed_fp, target_speed_fp, SPEED_EASE_DIVISOR);

    // speed_fp is fixed-point game-x units per second; scale the millisecond timestep down to
    // seconds while staying in integer math. An int64 intermediate avoids overflow: speed_fp
    // times a large dt_ms can exceed INT32_MAX well before dt_ms itself does.
    int64_t advance_fp = ((int64_t)state->speed_fp * dt_ms) / MS_PER_SECOND;
    state->distance_fp += (int32_t)advance_fp; // never negative: speed_fp never goes negative.

    int32_t new_x = state->distance_fp >> SIM_FP_SHIFT;
    state->vehicle_y = terrain_at(seed, new_x).height;
}
