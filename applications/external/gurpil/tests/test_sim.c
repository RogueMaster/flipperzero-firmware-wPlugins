#include "include/domain/shapes.h"
#include "include/domain/sim.h"
#include "include/domain/speed_ramp.h"
#include "include/domain/terrain.h"
#include "include/domain/terrain_kind.h"

#include <assert.h>
#include <stdio.h>

#define SEED_A 12345u
#define SEED_B 999u

// Fixed timestep used throughout: a plausible per-frame tick.
#define TICK_MS 15u

// How many ticks the "single-kind stretch" comparisons run for. Chosen (together with TICK_MS)
// so that even a full-speed shape can't travel far enough to leave a single terrain zone
// (zones are at least ZONE_LENGTH_MIN=20 game-x units long, from terrain.c).
#define COMPARE_TICK_COUNT 40

// Long-run tick count for the monotonicity/determinism checks, deliberately crossing many
// terrain zones.
#define LONG_RUN_TICK_COUNT 3000

// How many consecutive game-x units must share the desired kind for a "stable zone" search hit
// to be usable by the comparison tests below (must exceed how far COMPARE_TICK_COUNT ticks can
// possibly travel).
#define STABLE_SPAN 12

// Upper bound on how far the stable-zone search scans forward before giving up.
#define SEARCH_LIMIT 8000

// Stall advance must be less than 1/STALL_VS_LINE_RATIO of the line's advance over an obstacle.
#define STALL_VS_LINE_RATIO 3

// Number of ticks used for the "easing takes more than one tick" check.
#define EASE_MANY_TICKS 30

// Ticks to let speed settle onto its stage target; both stay within their zone (see asserts).
#define OPENING_PLATEAU_TICKS 25
#define FULL_PACE_TICKS 22

static void run_ticks(SimState *state, ShapeId shape, uint32_t seed, uint32_t tick_count) {
    for (uint32_t i = 0; i < tick_count; i++) {
        sim_step(state, shape, seed, TICK_MS);
    }
}

// Builds a state positioned at game-x `x`, as if the run had already travelled there, so tests
// can exercise a terrain zone that isn't at the very start of the course.
static SimState state_at(int32_t x, int16_t vehicle_y) {
    SimState state;
    state.distance_fp = x << SIM_FP_SHIFT;
    state.speed_fp = 0;
    state.vehicle_y = vehicle_y;
    return state;
}

// Scans forward from x=0 for the first x where `terrain_at(seed, x).kind == kind` holds for at
// least `span` consecutive positions — i.e. a zone stable enough that a short test run starting
// at x won't cross into the next zone. Real terrain_at output, no test seam / no mocks.
static int32_t find_stable_zone_start(uint32_t seed, TerrainKind kind, int32_t span) {
    for (int32_t x = 0; x < SEARCH_LIMIT; x++) {
        int32_t offset = 0;
        while (offset < span && terrain_at(seed, x + offset).kind == kind) {
            offset++;
        }
        if (offset == span) {
            return x;
        }
    }
    assert(0 && "no stable zone found for requested kind/span within SEARCH_LIMIT");
    return -1;
}

// Like find_stable_zone_start, but scanning from `min_x` so the hit lands in a chosen ramp
// stage (the opening flat zone is always stage 1, so higher stages need a floor).
static int32_t find_stable_zone_from(uint32_t seed, TerrainKind kind, int32_t span, int32_t min_x) {
    for (int32_t x = min_x; x < SEARCH_LIMIT; x++) {
        int32_t offset = 0;
        while (offset < span && terrain_at(seed, x + offset).kind == kind) {
            offset++;
        }
        if (offset == span) {
            return x;
        }
    }
    assert(0 && "no stable zone found for requested kind/span/min_x within SEARCH_LIMIT");
    return -1;
}

static void test_opening_pace_is_one_third_of_full(void) {
    // x=0 is always flat and stage 1, so circle eases toward SIM_MAX_SPEED_FP/3, never full pace.
    SimState state;
    sim_init(&state);
    run_ticks(&state, ShapeCircle, SEED_A, OPENING_PLATEAU_TICKS);

    int32_t stage1_target = SIM_MAX_SPEED_FP / SPEED_RAMP_MAX_STAGE;
    assert((state.distance_fp >> SIM_FP_SHIFT) < SPEED_RAMP_STAGE1_UNTIL);
    assert(state.speed_fp <= stage1_target);
    assert(state.speed_fp > stage1_target - SIM_FP_ONE);
    assert(state.speed_fp < SIM_MAX_SPEED_FP);
}

static void test_stage_three_reaches_full_unramped_pace(void) {
    // At stage 3 the multiplier is 3/3, so circle-on-flat climbs back to the pre-ramp top speed.
    int32_t flat_x =
        find_stable_zone_from(SEED_A, TerrainFlat, STABLE_SPAN, SPEED_RAMP_STAGE2_UNTIL);
    assert(speed_ramp_stage(flat_x) == SPEED_RAMP_MAX_STAGE);

    SimState state = state_at(flat_x, terrain_at(SEED_A, flat_x).height);
    run_ticks(&state, ShapeCircle, SEED_A, FULL_PACE_TICKS);

    assert(state.speed_fp <= SIM_MAX_SPEED_FP);
    assert(state.speed_fp > SIM_MAX_SPEED_FP - SIM_FP_ONE);
}

static void test_flat_best_shape_beats_poor_shape(void) {
    // x=0 is always TerrainFlat (the forced opening zone), regardless of seed.
    assert(terrain_at(SEED_A, 0).kind == TerrainFlat);

    SimState circle;
    sim_init(&circle);
    run_ticks(&circle, ShapeCircle, SEED_A, COMPARE_TICK_COUNT);

    SimState square;
    sim_init(&square);
    run_ticks(&square, ShapeSquare, SEED_A, COMPARE_TICK_COUNT);

    // Circle (FACTOR_BEST on flat) must cover strictly more distance than square (FACTOR_POOR).
    assert(circle.distance_fp > square.distance_fp);
}

static void test_obstacle_line_beats_stall_shapes(void) {
    int32_t obstacle_x = find_stable_zone_start(SEED_A, TerrainObstacle, STABLE_SPAN);
    int16_t start_height = terrain_at(SEED_A, obstacle_x).height;

    SimState line = state_at(obstacle_x, start_height);
    run_ticks(&line, ShapeLine, SEED_A, COMPARE_TICK_COUNT);
    int32_t line_advance = line.distance_fp - (obstacle_x << SIM_FP_SHIFT);

    SimState circle = state_at(obstacle_x, start_height);
    run_ticks(&circle, ShapeCircle, SEED_A, COMPARE_TICK_COUNT);
    int32_t circle_advance = circle.distance_fp - (obstacle_x << SIM_FP_SHIFT);

    SimState triangle = state_at(obstacle_x, start_height);
    run_ticks(&triangle, ShapeTriangle, SEED_A, COMPARE_TICK_COUNT);
    int32_t triangle_advance = triangle.distance_fp - (obstacle_x << SIM_FP_SHIFT);

    // Line passes through the obstacle; the other shapes stall near-zero.
    assert(line_advance > 0);
    assert(circle_advance < SIM_FP_ONE);
    assert(triangle_advance < SIM_FP_ONE);
    assert(circle_advance * STALL_VS_LINE_RATIO < line_advance);
    assert(triangle_advance * STALL_VS_LINE_RATIO < line_advance);
}

static void test_distance_is_monotonic_non_decreasing(void) {
    SimState state;
    sim_init(&state);
    int32_t previous = state.distance_fp;
    for (uint32_t i = 0; i < LONG_RUN_TICK_COUNT; i++) {
        // Alternate shapes so the run crosses zones of every kind under different shapes.
        ShapeId shape = (ShapeId)(i % ShapeCount);
        sim_step(&state, shape, SEED_A, TICK_MS);
        assert(state.distance_fp >= previous);
        previous = state.distance_fp;
    }
}

static void test_deterministic_given_same_inputs(void) {
    SimState a;
    SimState b;
    sim_init(&a);
    sim_init(&b);
    for (uint32_t i = 0; i < LONG_RUN_TICK_COUNT; i++) {
        ShapeId shape = (ShapeId)(i % ShapeCount);
        sim_step(&a, shape, SEED_A, TICK_MS);
        sim_step(&b, shape, SEED_A, TICK_MS);
        assert(a.distance_fp == b.distance_fp);
        assert(a.speed_fp == b.speed_fp);
        assert(a.vehicle_y == b.vehicle_y);
    }
}

static void test_different_seed_diverges_eventually(void) {
    SimState a;
    SimState b;
    sim_init(&a);
    sim_init(&b);
    int diverged = 0;
    for (uint32_t i = 0; i < LONG_RUN_TICK_COUNT; i++) {
        sim_step(&a, ShapeCircle, SEED_A, TICK_MS);
        sim_step(&b, ShapeCircle, SEED_B, TICK_MS);
        if (a.distance_fp != b.distance_fp) {
            diverged = 1;
            break;
        }
    }
    assert(diverged);
}

static void test_speed_eases_toward_target_over_ticks(void) {
    // x=0 is always flat, and circle is flat's best shape, so the target speed is high and
    // constant across every tick of this run (same kind, same shape).
    SimState state;
    sim_init(&state);

    sim_step(&state, ShapeCircle, SEED_A, TICK_MS);
    int32_t speed_after_one_tick = state.speed_fp;

    run_ticks(&state, ShapeCircle, SEED_A, EASE_MANY_TICKS - 1);
    int32_t speed_after_many_ticks = state.speed_fp;

    // Speed ramps up tick by tick rather than jumping straight to the target in one step.
    assert(speed_after_one_tick > 0);
    assert(speed_after_one_tick < speed_after_many_ticks);
}

int main(void) {
    test_opening_pace_is_one_third_of_full();
    printf("test_opening_pace_is_one_third_of_full: PASS\n");

    test_stage_three_reaches_full_unramped_pace();
    printf("test_stage_three_reaches_full_unramped_pace: PASS\n");

    test_flat_best_shape_beats_poor_shape();
    printf("test_flat_best_shape_beats_poor_shape: PASS\n");

    test_obstacle_line_beats_stall_shapes();
    printf("test_obstacle_line_beats_stall_shapes: PASS\n");

    test_distance_is_monotonic_non_decreasing();
    printf("test_distance_is_monotonic_non_decreasing: PASS\n");

    test_deterministic_given_same_inputs();
    printf("test_deterministic_given_same_inputs: PASS\n");

    test_different_seed_diverges_eventually();
    printf("test_different_seed_diverges_eventually: PASS\n");

    test_speed_eases_toward_target_over_ticks();
    printf("test_speed_eases_toward_target_over_ticks: PASS\n");

    return 0;
}
