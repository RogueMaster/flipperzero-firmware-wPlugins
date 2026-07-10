#pragma once

#include <stdint.h>

/*
 * Pure, deterministic endless-mode checkpoint timer (no furi, no I/O, no floats).
 *
 * The vehicle never "dies" from the terrain in endless mode: the only fail condition is running
 * out of time. The run starts with a time budget; every ENDLESS_CHECKPOINT_SPACING units of
 * distance tops the timer back up (capped at ENDLESS_MAX_TIME_MS). The top-up SHRINKS with each
 * checkpoint already crossed (endless_checkpoint_bonus_ms) — a rising-difficulty treadmill: early
 * on the bonus exceeds the time it takes to cross a gap so good play banks time, but as the bonus
 * decays past that crossing time even optimal play bleeds and eventually runs out, so every run is
 * finite and distance is the score. The caller owns the clock and the sim's distance; this module
 * only advances state given an elapsed `dt_ms` and the current distance.
 */

typedef enum {
    EndlessIdle,    // not started yet: endless_init's resting state.
    EndlessRunning, // timer depleting, distance/checkpoints tracked.
    EndlessOver,    // time_left_ms hit 0: run is frozen, score is `distance`.
} EndlessPhase;

// Tuning constants. Public so the app/UI layer can read the exact same values it displays.
enum {
    ENDLESS_START_TIME_MS = 6000,             // initial time budget for a fresh run, ms.
    ENDLESS_CHECKPOINT_BONUS_BASE_MS = 3000,  // bonus for the very first checkpoint, ms.
    ENDLESS_CHECKPOINT_BONUS_FLOOR_MS = 1500, // bonus never decays below this, ms.
    ENDLESS_CHECKPOINT_BONUS_DECAY_MS = 40,   // bonus lost per checkpoint already crossed, ms.
    ENDLESS_CHECKPOINT_SPACING = 45,          // distance units between checkpoints.
    ENDLESS_MAX_TIME_MS = 7000,               // hard cap on time_left_ms; bonuses never exceed it.
};

typedef struct {
    EndlessPhase phase;
    uint32_t time_left_ms;    // remaining time budget; saturates at 0, never wraps.
    int32_t distance;         // best (monotonic non-decreasing) distance reached so far.
    int32_t next_checkpoint;  // distance threshold that triggers the next time bonus.
    uint32_t checkpoints_hit; // total checkpoints crossed so far, for scoring/UI.
    uint32_t last_bonus_ms;   // time bonus granted by the most recent checkpoint, for the UI flash.
} EndlessState;

/* The (decaying) time bonus a checkpoint grants, given how many were already crossed before it:
 * ENDLESS_CHECKPOINT_BONUS_BASE_MS for the first (checkpoints_hit == 0), decreasing by
 * ENDLESS_CHECKPOINT_BONUS_DECAY_MS per prior checkpoint, floored at
 * ENDLESS_CHECKPOINT_BONUS_FLOOR_MS. Pure; the single source of truth for both endless_tick and the
 * HUD flash. */
uint32_t endless_checkpoint_bonus_ms(uint32_t checkpoints_hit);

/* Resets `state` to a run's start: EndlessIdle; time_left_ms = ENDLESS_START_TIME_MS;
 * distance = 0; next_checkpoint = ENDLESS_CHECKPOINT_SPACING; checkpoints_hit = 0;
 * last_bonus_ms = 0. */
void endless_init(EndlessState *state);

/* Moves `state` from EndlessIdle to EndlessRunning. No-op if not currently Idle. */
void endless_start(EndlessState *state);

/* Advances `state` by one tick of `dt_ms` milliseconds, given the sim's current `distance`.
 * No-op unless `state->phase == EndlessRunning` (a call while Idle or Over does nothing).
 * Effects, in order:
 *   1. `state->distance` becomes max(state->distance, distance) — monotonic, never regresses
 *      even if the caller passes a smaller distance than already recorded.
 *   2. `time_left_ms` is depleted by `dt_ms`, saturating at 0 (never underflows/wraps).
 *   3. While `state->distance >= next_checkpoint`: add endless_checkpoint_bonus_ms(checkpoints_hit)
 *      to time_left_ms (capped at ENDLESS_MAX_TIME_MS) and into last_bonus_ms, advance
 *      next_checkpoint by ENDLESS_CHECKPOINT_SPACING, and increment checkpoints_hit. Looped so
 *      several checkpoints crossed in one big jump each grant their own (decaying) bonus.
 *   4. If `time_left_ms == 0`, phase becomes EndlessOver (freezing distance/checkpoints: later
 *      ticks are no-ops per the guard above). */
void endless_tick(EndlessState *state, uint32_t dt_ms, int32_t distance);
