#include "include/application/game.h"

#include "include/domain/speed_ramp.h"
#include "include/domain/terrain.h"

enum {
    GAME_SPEED_PERMILLE_SCALE = 1000, // output scale for game_speed_permille: 1000 == 100% of
                                      // SIM_MAX_SPEED_FP (sim.h).

    TUTORIAL_CHECKPOINT_COUNT = 3, // the hint stays on for the run's first few checkpoints, long
                                   // enough to teach the mechanic, then fades out.
    TUTORIAL_DISTANCE = ENDLESS_CHECKPOINT_SPACING * TUTORIAL_CHECKPOINT_COUNT,

    TUTORIAL_HINT_LOOKAHEAD = 10, // distance units ahead of the vehicle the hint samples, so the
                                  // suggested shape reflects terrain about to arrive rather than
                                  // just the exact spot underfoot.
};

void game_start(GameState *game, uint32_t seed) {
    game->seed = seed;
    sim_init(&game->sim);
    endless_init(&game->endless);
    game->shape = ShapeCircle;
    game->checkpoints_hit_before_tick = 0;
    endless_start(&game->endless);
}

void game_set_shape(GameState *game, ShapeId shape) {
    if (shape >= ShapeCount) {
        return;
    }
    game->shape = shape;
}

void game_tick(GameState *game, uint32_t dt_ms) {
    if (game->endless.phase != EndlessRunning) {
        return;
    }

    game->checkpoints_hit_before_tick = game->endless.checkpoints_hit;

    sim_step(&game->sim, game->shape, game->seed, dt_ms);

    int32_t distance = game->sim.distance_fp >> SIM_FP_SHIFT;
    endless_tick(&game->endless, dt_ms, distance);
}

int32_t game_distance(const GameState *game) {
    return game->endless.distance;
}

uint32_t game_time_left(const GameState *game) {
    return game->endless.time_left_ms;
}

int game_is_over(const GameState *game) {
    return game->endless.phase == EndlessOver;
}

int16_t game_vehicle_y(const GameState *game) {
    return game->sim.vehicle_y;
}

uint16_t game_speed_permille(const GameState *game) {
    int32_t speed_fp = game->sim.speed_fp;
    if (speed_fp <= 0) {
        return 0;
    }

    int64_t permille = ((int64_t)speed_fp * GAME_SPEED_PERMILLE_SCALE) / SIM_MAX_SPEED_FP;
    if (permille > GAME_SPEED_PERMILLE_SCALE) {
        // Defensive clamp: speed_fp should never exceed SIM_MAX_SPEED_FP (sim_step's easing
        // never overshoots its target, and the target itself is capped by shape_speed_factor's
        // own 0..256 scale), but an out-of-range value silently wrapping into a uint16_t would
        // be worse than a clamp.
        permille = GAME_SPEED_PERMILLE_SCALE;
    }
    return (uint16_t)permille;
}

uint8_t game_speed_stage(const GameState *game) {
    return speed_ramp_stage(game_distance(game));
}

bool game_checkpoint_just_hit(const GameState *game) {
    return game->endless.checkpoints_hit > game->checkpoints_hit_before_tick;
}

ShapeId game_hint_shape(const GameState *game) {
    int32_t lookahead_x = game_distance(game) + TUTORIAL_HINT_LOOKAHEAD;
    TerrainKind kind = terrain_at(game->seed, lookahead_x).kind;
    return shape_best_for(kind);
}

bool game_hint_active(const GameState *game) {
    return game_distance(game) < TUTORIAL_DISTANCE;
}

bool game_is_new_best(const GameState *game, int32_t pre_run_best) {
    return game_distance(game) > pre_run_best;
}
