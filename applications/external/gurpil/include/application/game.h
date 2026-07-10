#pragma once

#include "include/domain/endless.h"
#include "include/domain/shapes.h"
#include "include/domain/sim.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Pure per-tick orchestrator (no furi, no I/O, integer only) tying together the sim (distance/
 * speed/vehicle height), the endless checkpoint timer, and the player-mounted wheel shape into
 * one game state. The caller (UI/hardware layer) owns the real time source and the RNG seed;
 * this module never reads either itself — both arrive as plain arguments.
 *
 * Design choice: `game_start` moves endless straight to EndlessRunning instead of leaving it
 * Idle until the player's first shape input. Endless mode's whole point is the timer counting
 * down from the very start of a run, so the countdown should be live immediately; there is no
 * "waiting room" phase. `game_tick` only advances the sim while endless is Running (see below),
 * so the vehicle simply doesn't move until the caller starts ticking — an idle player loses time
 * but not distance.
 */

typedef struct {
    SimState sim;
    EndlessState endless;
    ShapeId shape;
    uint32_t seed;
    // endless.checkpoints_hit as of just before the most recent game_tick call, so
    // game_checkpoint_just_hit can detect a same-tick increment without the render layer
    // needing its own frame-diffing state.
    uint32_t checkpoints_hit_before_tick;
} GameState;

/* Resets `game` to a fresh run: stores `seed` (used by every later sim_step); sim_init and
 * endless_init reset their substates; shape defaults to ShapeCircle; endless is immediately
 * moved to EndlessRunning (see the module comment above for why). */
void game_start(GameState *game, uint32_t seed);

/* Mounts `shape`, effective from the very next game_tick. Out-of-range values (>= ShapeCount)
 * are ignored, leaving the previously mounted shape unchanged. */
void game_set_shape(GameState *game, ShapeId shape);

/* Advances `game` by one fixed step of `dt_ms` milliseconds:
 *   1. no-op unless `game->endless.phase == EndlessRunning` — once endless is Over, the sim is
 *      frozen too, so the whole orchestrator stops, not just the timer.
 *   2. sim_step(&game->sim, game->shape, game->seed, dt_ms) advances distance/speed/vehicle_y.
 *   3. the sim's integer distance feeds endless_tick, which may grant checkpoint bonuses or end
 *      the run. */
void game_tick(GameState *game, uint32_t dt_ms);

/* Best (monotonic non-decreasing) integer distance reached so far this run. */
int32_t game_distance(const GameState *game);

/* Remaining time budget, in milliseconds; 0 once the run is over. */
uint32_t game_time_left(const GameState *game);

/* Non-zero once the run's timer has hit 0 (EndlessOver): distance/time are frozen. */
int game_is_over(const GameState *game);

/* Terrain height (game units) under the vehicle at its current position. */
int16_t game_vehicle_y(const GameState *game);

/* Current speed as 0..1000 (permille) of the fastest speed a run can ever reach
 * (SIM_MAX_SPEED_FP, see domain/sim.h) — the pure value the HUD speed bar fills with, so
 * changing the mounted shape visibly moves the bar. 0 while stalled/stopped; 1000 only at the
 * theoretical max (an ideal shape on its best terrain, fully eased up to speed). */
uint16_t game_speed_permille(const GameState *game);

/* Current speed-ramp stage (1..SPEED_RAMP_MAX_STAGE) for the run's distance so far: the game
 * opens at stage 1 (1/3 pace) and climbs to full pace by the top stage. The HUD shows this as
 * x1/x2/x3; the sim reads the same speed_ramp_stage, so screen and pace never disagree. */
uint8_t game_speed_stage(const GameState *game);

/* True exactly when the most recent game_tick call crossed at least one checkpoint (i.e.
 * endless.checkpoints_hit increased during that call) — the trigger for a brief on-screen
 * "+Ns" flash. False before the first tick and on every tick that doesn't cross a checkpoint. */
bool game_checkpoint_just_hit(const GameState *game);

/* The ideal wheel shape (shape_best_for) for the terrain a short lookahead ahead of the
 * vehicle's current position — the shape the tutorial hint suggests mounting. */
ShapeId game_hint_shape(const GameState *game);

/* True while the run is still within the tutorial window (an early stretch of distance, long
 * enough to cover a handful of checkpoints) during which the hint icon should be shown; false
 * once the player has travelled far enough that the hint fades out. */
bool game_hint_active(const GameState *game);

/* True once this run's distance has strictly exceeded `pre_run_best` — the best distance
 * recorded before this run started. GameState never stores or reads the persisted best itself;
 * the caller (which owns best_store) supplies it. */
bool game_is_new_best(const GameState *game, int32_t pre_run_best);
