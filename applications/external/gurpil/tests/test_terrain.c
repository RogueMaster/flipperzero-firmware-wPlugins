#include "include/domain/terrain.h"
#include "include/domain/terrain_kind.h"

#include <assert.h>
#include <stdio.h>

#define SEED_A 12345u
#define SEED_B 999u

#define LONG_SPAN_END 20000
#define OPENING_SPAN_END 500
#define FAR_SPAN_START 15000
#define FAR_SPAN_END 20000
// Prime-ish stride so sampled x values land at varied offsets within their zones, instead of
// always hitting the same phase.
#define SAMPLE_STEP 7
#define RNG_SAMPLE_COUNT 100

static void test_terrain_at_is_deterministic(void) {
    for (int32_t x = 0; x < LONG_SPAN_END; x += SAMPLE_STEP) {
        TerrainSample first = terrain_at(SEED_A, x);
        TerrainSample second = terrain_at(SEED_A, x);
        assert(first.kind == second.kind);
        assert(first.height == second.height);
    }
}

static void test_different_seeds_diverge(void) {
    int diverged = 0;
    for (int32_t x = 0; x < LONG_SPAN_END; x += SAMPLE_STEP) {
        TerrainSample a = terrain_at(SEED_A, x);
        TerrainSample b = terrain_at(SEED_B, x);
        if (a.kind != b.kind || a.height != b.height) {
            diverged = 1;
            break;
        }
    }
    assert(diverged);
}

static void test_all_kinds_appear_over_long_span(void) {
    int seen[TerrainKindCount] = {0};
    for (int32_t x = 0; x < LONG_SPAN_END; x += SAMPLE_STEP) {
        seen[terrain_at(SEED_A, x).kind] = 1;
    }
    for (int kind = 0; kind < TerrainKindCount; kind++) {
        assert(seen[kind]);
    }
}

static void test_heights_stay_within_bounds(void) {
    for (int32_t x = 0; x < LONG_SPAN_END; x += SAMPLE_STEP) {
        int16_t height = terrain_at(SEED_A, x).height;
        assert(height >= TERRAIN_HEIGHT_MIN);
        assert(height <= TERRAIN_HEIGHT_MAX);
    }
}

static int is_hard_kind(TerrainKind kind) {
    return kind == TerrainUphill || kind == TerrainObstacle;
}

// Fraction of sampled positions in [start, end) that fall on a "hard" zone (uphill/obstacle).
static double hard_fraction(uint32_t seed, int32_t start, int32_t end) {
    int hard = 0;
    int total = 0;
    for (int32_t x = start; x < end; x += SAMPLE_STEP) {
        total++;
        if (is_hard_kind(terrain_at(seed, x).kind)) {
            hard++;
        }
    }
    return (double)hard / (double)total;
}

static void test_difficulty_ramps_with_distance(void) {
    double opening = hard_fraction(SEED_A, 0, OPENING_SPAN_END);
    double far = hard_fraction(SEED_A, FAR_SPAN_START, FAR_SPAN_END);
    assert(far > opening);
}

static void test_opening_is_flat(void) {
    assert(terrain_at(SEED_A, 0).kind == TerrainFlat);
    // Independent of seed: the opening zones are forced flat regardless of the PRNG draw.
    assert(terrain_at(SEED_B, 0).kind == TerrainFlat);
}

static void test_rng_same_seed_same_sequence(void) {
    uint32_t state_a = gurpil_rng_seed(SEED_A);
    uint32_t state_b = gurpil_rng_seed(SEED_A);
    for (int i = 0; i < RNG_SAMPLE_COUNT; i++) {
        assert(gurpil_rng_next(&state_a) == gurpil_rng_next(&state_b));
    }
}

static void test_rng_is_not_stuck(void) {
    uint32_t state = gurpil_rng_seed(SEED_A);
    uint32_t first = gurpil_rng_next(&state);
    int saw_different = 0;
    for (int i = 0; i < RNG_SAMPLE_COUNT; i++) {
        if (gurpil_rng_next(&state) != first) {
            saw_different = 1;
        }
    }
    assert(saw_different);
}

int main(void) {
    test_terrain_at_is_deterministic();
    printf("test_terrain_at_is_deterministic: PASS\n");

    test_different_seeds_diverge();
    printf("test_different_seeds_diverge: PASS\n");

    test_all_kinds_appear_over_long_span();
    printf("test_all_kinds_appear_over_long_span: PASS\n");

    test_heights_stay_within_bounds();
    printf("test_heights_stay_within_bounds: PASS\n");

    test_difficulty_ramps_with_distance();
    printf("test_difficulty_ramps_with_distance: PASS\n");

    test_opening_is_flat();
    printf("test_opening_is_flat: PASS\n");

    test_rng_same_seed_same_sequence();
    printf("test_rng_same_seed_same_sequence: PASS\n");

    test_rng_is_not_stuck();
    printf("test_rng_is_not_stuck: PASS\n");

    return 0;
}
