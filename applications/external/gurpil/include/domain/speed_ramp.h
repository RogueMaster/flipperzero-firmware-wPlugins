#pragma once

#include <stdint.h>

/*
 * Pure, deterministic distance-driven speed ramp (no furi, no floats). The sim scales its
 * target speed by stage/SPEED_RAMP_MAX_STAGE, so a run opens at 1/3 pace and builds up to the
 * full, unchanged top speed by stage 3 (x3 == the pace before this ramp existed). The sim and
 * the HUD both read speed_ramp_stage for the same distance, so the on-screen x1/x2/x3 always
 * matches the pace under the wheel.
 */
enum {
    SPEED_RAMP_MAX_STAGE = 3,

    // Distance thresholds in game-x units: stage 1 below STAGE1_UNTIL, stage 2 below
    // STAGE2_UNTIL, stage 3 beyond. Sized so the slow opening still clears the first
    // checkpoints inside endless's 6 s budget (no endless retuning needed); raise both for a
    // longer build-up.
    SPEED_RAMP_STAGE1_UNTIL = 30,
    SPEED_RAMP_STAGE2_UNTIL = 120,
};

/* The speed stage (1..SPEED_RAMP_MAX_STAGE) for a distance travelled, in game-x units.
 * Monotonic non-decreasing in distance; a zero or negative distance maps to stage 1. Pure. */
uint8_t speed_ramp_stage(int32_t distance_units);
