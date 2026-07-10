#include "include/application/game.h"

#include "include/domain/shapes.h"
#include "include/domain/sim.h"
#include "include/domain/speed_ramp.h"
#include "include/domain/terrain.h"
#include "include/domain/terrain_kind.h"

#include <assert.h>
#include <stdio.h>

#define SEED_A 12345u

// Fixed per-frame timestep, matching the plausible tick used by the sim's own test suite.
#define TICK_MS 15u

// Fixed "wrong" shape mounted for the entire dumb run. It is never `shape_best_for` any
// terrain kind (shapes.c's TABLE has circle/line/triangle as the sole best for flat/rocky-or-
// obstacle/uphill respectively), so a run that never adapts away from it is consistently
// suboptimal.
#define DUMB_SHAPE ShapeSquare

// Generous upper bound for a fixed poor shape to run out of time and reach game-over.
#define MAX_TICKS 20000u

// A well-played run is expected to still be alive this far in (see the sustain test below): far
// past where a fixed poor shape has died, but well short of where even optimal play hits the wall.
#define SUSTAINED_TICKS 1200u

// How many post-game-over ticks to drive in the freeze check.
#define POST_OVER_TICK_COUNT 50u

// ENDLESS_START_TIME_MS is public in endless.h (single source of truth); asserted directly
// against game_start's resulting state below.

// Ticks driven before sampling game_speed_permille, so speed_fp has eased close enough to its
// target for the high/low assertions to hold comfortably (see sim.c's SPEED_EASE_DIVISOR).
#define SPEED_RAMP_TICK_COUNT 30u

// Comfortable margins for the speed-bar permille assertions: well above "empty" for a
// best-matched shape, well below "full" for a stalled one, with room to spare either way.
#define SPEED_PERMILLE_HIGH_THRESHOLD 700u
#define SPEED_PERMILLE_LOW_THRESHOLD 100u

// How many consecutive game-x units must share TerrainObstacle for the stable-zone search below
// to be usable (mirrors test_sim.c's own STABLE_SPAN/SEARCH_LIMIT pattern).
#define OBSTACLE_STABLE_SPAN 12
#define OBSTACLE_SEARCH_LIMIT 8000

// A flat span long enough that SPEED_RAMP_TICK_COUNT ticks at full pace stay inside the zone.
#define FLAT_STABLE_SPAN 16

// Ticks of smart play driven to push the run past the first ramp threshold (out of stage 1).
#define SPEED_STAGE_DRIVE_TICKS 300u

// Drives `game` for up to `ticks`, each tick mounting the best shape for the current terrain.
static void run_smart_for(GameState *game, uint32_t seed, uint32_t ticks) {
    for (uint32_t i = 0; i < ticks && !game_is_over(game); i++) {
        int32_t x = game->sim.distance_fp >> SIM_FP_SHIFT;
        ShapeId best = shape_best_for(terrain_at(seed, x).kind);
        game_set_shape(game, best);
        game_tick(game, TICK_MS);
    }
}

// Drives `game` with a single fixed shape for the whole run, until endless goes Over or
// MAX_TICKS is exhausted.
static void run_fixed_shape_until_over(GameState *game, ShapeId shape) {
    game_set_shape(game, shape);
    for (uint32_t i = 0; i < MAX_TICKS && !game_is_over(game); i++) {
        game_tick(game, TICK_MS);
    }
}

static void test_adapting_shape_sustains_run_far_beyond_fixed_poor_shape(void) {
    // A fixed poor shape runs out of time; adapting the shape to the terrain is still alive well
    // past that point and has travelled strictly further. Skill keeps you going — the game's point.
    GameState dumb;
    game_start(&dumb, SEED_A);
    run_fixed_shape_until_over(&dumb, DUMB_SHAPE);
    assert(game_is_over(&dumb));

    GameState smart;
    game_start(&smart, SEED_A);
    run_smart_for(&smart, SEED_A, SUSTAINED_TICKS);
    assert(!game_is_over(&smart));
    assert(game_distance(&smart) > game_distance(&dumb));
}

static void test_over_freezes_distance_through_orchestrator(void) {
    GameState game;
    game_start(&game, SEED_A);
    run_fixed_shape_until_over(&game, DUMB_SHAPE);
    assert(game_is_over(&game));

    int32_t frozen_distance = game_distance(&game);
    uint32_t frozen_time = game_time_left(&game);

    // Further ticks after game-over must not move the run forward at all: the orchestrator's
    // own Running-only guard, not just endless_tick's internal one.
    for (uint32_t i = 0; i < POST_OVER_TICK_COUNT; i++) {
        game_tick(&game, TICK_MS);
    }

    assert(game_is_over(&game));
    assert(game_distance(&game) == frozen_distance);
    assert(game_time_left(&game) == frozen_time);
}

static void test_deterministic_given_same_inputs(void) {
    GameState a;
    GameState b;
    game_start(&a, SEED_A);
    game_start(&b, SEED_A);

    for (uint32_t i = 0; i < MAX_TICKS && !game_is_over(&a); i++) {
        // Same shape sequence fed to both runs: cycle through every shape so it isn't just the
        // default.
        ShapeId shape = (ShapeId)(i % ShapeCount);
        game_set_shape(&a, shape);
        game_set_shape(&b, shape);
        game_tick(&a, TICK_MS);
        game_tick(&b, TICK_MS);
        assert(game_distance(&a) == game_distance(&b));
        assert(game_time_left(&a) == game_time_left(&b));
        assert(game_is_over(&a) == game_is_over(&b));
    }

    assert(game_distance(&a) == game_distance(&b));
    assert(game_time_left(&a) == game_time_left(&b));
}

static void test_start_initializes_coherent_state(void) {
    GameState game;
    game_start(&game, SEED_A);

    assert(game_distance(&game) == 0);
    assert(game_time_left(&game) == (uint32_t)ENDLESS_START_TIME_MS);
    assert(!game_is_over(&game));
    assert(game.shape == ShapeCircle);
    assert(game.endless.phase == EndlessRunning);
    // x=0 is always the flat opening zone (terrain.c), so the vehicle starts at that height.
    assert(game_vehicle_y(&game) == terrain_at(SEED_A, 0).height);
}

// Scans forward from x=0 for the first x where TerrainObstacle holds for at least `span`
// consecutive positions — mirrors test_sim.c's find_stable_zone_start. Real terrain_at output,
// no test seam.
static int32_t find_stable_obstacle_start(uint32_t seed, int32_t span) {
    for (int32_t x = 0; x < OBSTACLE_SEARCH_LIMIT; x++) {
        int32_t offset = 0;
        while (offset < span && terrain_at(seed, x + offset).kind == TerrainObstacle) {
            offset++;
        }
        if (offset == span) {
            return x;
        }
    }
    assert(0 && "no stable obstacle zone found within OBSTACLE_SEARCH_LIMIT");
    return -1;
}

// Scans from min_x for the first x with TerrainFlat stable for `span` positions — mirrors
// find_stable_obstacle_start, used to place the vehicle in a stage-3 flat zone.
static int32_t find_stable_flat_from(uint32_t seed, int32_t span, int32_t min_x) {
    for (int32_t x = min_x; x < OBSTACLE_SEARCH_LIMIT; x++) {
        int32_t offset = 0;
        while (offset < span && terrain_at(seed, x + offset).kind == TerrainFlat) {
            offset++;
        }
        if (offset == span) {
            return x;
        }
    }
    assert(0 && "no stable flat zone found within OBSTACLE_SEARCH_LIMIT");
    return -1;
}

static void test_speed_permille_is_high_for_a_best_matched_shape(void) {
    // The ramp caps the opening at 1/3, so a full bar is only reachable at full pace (stage 3);
    // jump to a flat stage-3 zone where circle (flat's best shape) fills it.
    int32_t flat_x = find_stable_flat_from(SEED_A, FLAT_STABLE_SPAN, SPEED_RAMP_STAGE2_UNTIL);

    GameState game;
    game_start(&game, SEED_A);
    game.sim.distance_fp = flat_x << SIM_FP_SHIFT;
    game.sim.vehicle_y = terrain_at(SEED_A, flat_x).height;
    game_set_shape(&game, ShapeCircle);
    for (uint32_t i = 0; i < SPEED_RAMP_TICK_COUNT; i++) {
        game_tick(&game, TICK_MS);
    }

    assert(game_speed_permille(&game) > SPEED_PERMILLE_HIGH_THRESHOLD);
}

static void test_speed_stage_matches_ramp_and_climbs_out_of_the_opening(void) {
    GameState game;
    game_start(&game, SEED_A);
    assert(game_speed_stage(&game) == 1);

    run_smart_for(&game, SEED_A, SPEED_STAGE_DRIVE_TICKS);
    assert(game_speed_stage(&game) == speed_ramp_stage(game_distance(&game)));
    assert(game_distance(&game) >= SPEED_RAMP_STAGE1_UNTIL);
    assert(game_speed_stage(&game) >= 2);
}

static void test_speed_permille_is_near_zero_for_a_stalled_shape(void) {
    int32_t obstacle_x = find_stable_obstacle_start(SEED_A, OBSTACLE_STABLE_SPAN);

    GameState game;
    game_start(&game, SEED_A);
    // Jump the sim straight to the obstacle zone (public fields, same pattern as test_sim.c's
    // state_at) instead of ticking there tick by tick.
    game.sim.distance_fp = obstacle_x << SIM_FP_SHIFT;
    game.sim.vehicle_y = terrain_at(SEED_A, obstacle_x).height;
    game_set_shape(&game, ShapeCircle); // circle stalls on obstacle (shapes.c's FACTOR_STALL).
    for (uint32_t i = 0; i < SPEED_RAMP_TICK_COUNT; i++) {
        game_tick(&game, TICK_MS);
    }

    assert(game_speed_permille(&game) < SPEED_PERMILLE_LOW_THRESHOLD);
}

static void test_checkpoint_just_hit_is_true_only_on_the_crossing_tick(void) {
    GameState game;
    game_start(&game, SEED_A);

    assert(!game_checkpoint_just_hit(&game)); // nothing crossed before the first tick.

    // Jump distance to exactly the first checkpoint threshold, then tick: endless_tick's own
    // distance-max means this single tick both registers the jump and crosses the checkpoint.
    game.sim.distance_fp = ENDLESS_CHECKPOINT_SPACING << SIM_FP_SHIFT;
    game_tick(&game, TICK_MS);
    assert(game_checkpoint_just_hit(&game));

    // The very next tick, still short of the next checkpoint, must not report a fresh crossing.
    game_tick(&game, TICK_MS);
    assert(!game_checkpoint_just_hit(&game));
}

static void test_hint_shape_matches_best_for_upcoming_terrain(void) {
    GameState game;
    game_start(&game, SEED_A);

    // x=0's opening zone is flat and at least ZONE_LENGTH_MIN=20 units long (terrain.c), so the
    // hint's small lookahead still lands inside that same flat zone right at a run's start.
    assert(game_hint_shape(&game) == shape_best_for(TerrainFlat));
}

static void test_hint_active_at_start_then_turns_off_with_distance(void) {
    GameState game;
    game_start(&game, SEED_A);
    assert(game_hint_active(&game));

    // Drive a well-played run, adapting shape every tick, until the hint switches off or the run
    // ends — whichever comes first. A well-played run outlasts the (finite) tutorial window, so
    // seeing hint_active go false proves the hint actually turns off with distance rather than
    // just staying on for the whole run.
    bool saw_hint_turn_off = false;
    for (uint32_t i = 0; i < MAX_TICKS && !game_is_over(&game); i++) {
        int32_t x = game.sim.distance_fp >> SIM_FP_SHIFT;
        ShapeId best = shape_best_for(terrain_at(SEED_A, x).kind);
        game_set_shape(&game, best);
        game_tick(&game, TICK_MS);
        if (!game_hint_active(&game)) {
            saw_hint_turn_off = true;
            break;
        }
    }

    assert(saw_hint_turn_off);
}

static void test_is_new_best_true_only_when_distance_exceeds_pre_run_best(void) {
    GameState game;
    game_start(&game, SEED_A);
    game.endless.distance = 100; // direct field set: simulate a run already at 100m.

    assert(game_is_new_best(&game, 50));   // strictly beats the old best.
    assert(!game_is_new_best(&game, 100)); // a tie is not a new best.
    assert(!game_is_new_best(&game, 150)); // still short of the old best.
}

int main(void) {
    test_adapting_shape_sustains_run_far_beyond_fixed_poor_shape();
    printf("test_adapting_shape_sustains_run_far_beyond_fixed_poor_shape: PASS\n");

    test_over_freezes_distance_through_orchestrator();
    printf("test_over_freezes_distance_through_orchestrator: PASS\n");

    test_deterministic_given_same_inputs();
    printf("test_deterministic_given_same_inputs: PASS\n");

    test_start_initializes_coherent_state();
    printf("test_start_initializes_coherent_state: PASS\n");

    test_speed_permille_is_high_for_a_best_matched_shape();
    printf("test_speed_permille_is_high_for_a_best_matched_shape: PASS\n");

    test_speed_permille_is_near_zero_for_a_stalled_shape();
    printf("test_speed_permille_is_near_zero_for_a_stalled_shape: PASS\n");

    test_speed_stage_matches_ramp_and_climbs_out_of_the_opening();
    printf("test_speed_stage_matches_ramp_and_climbs_out_of_the_opening: PASS\n");

    test_checkpoint_just_hit_is_true_only_on_the_crossing_tick();
    printf("test_checkpoint_just_hit_is_true_only_on_the_crossing_tick: PASS\n");

    test_hint_shape_matches_best_for_upcoming_terrain();
    printf("test_hint_shape_matches_best_for_upcoming_terrain: PASS\n");

    test_hint_active_at_start_then_turns_off_with_distance();
    printf("test_hint_active_at_start_then_turns_off_with_distance: PASS\n");

    test_is_new_best_true_only_when_distance_exceeds_pre_run_best();
    printf("test_is_new_best_true_only_when_distance_exceeds_pre_run_best: PASS\n");

    return 0;
}
