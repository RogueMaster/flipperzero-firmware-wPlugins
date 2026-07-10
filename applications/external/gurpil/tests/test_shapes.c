#include "include/domain/shapes.h"
#include "include/domain/terrain_kind.h"

#include <assert.h>
#include <stdio.h>

#define STALL_THRESHOLD 32 // factors below this are considered "near-zero" (stalled)

static void test_every_factor_in_range(void) {
    for (ShapeId shape = ShapeCircle; shape < ShapeCount; shape++) {
        for (TerrainKind kind = TerrainFlat; kind < TerrainKindCount; kind++) {
            uint16_t factor = shape_speed_factor(shape, kind);
            assert(factor <= 256);
        }
    }
}

// For a given terrain kind, returns how many shapes tie the maximal factor. Used to assert
// each terrain has exactly one strictly-best shape.
static int count_maximal_shapes(TerrainKind kind, uint16_t max_factor) {
    int count = 0;
    for (ShapeId shape = ShapeCircle; shape < ShapeCount; shape++) {
        if (shape_speed_factor(shape, kind) == max_factor) {
            count++;
        }
    }
    return count;
}

static uint16_t max_factor_for(TerrainKind kind) {
    uint16_t max_factor = 0;
    for (ShapeId shape = ShapeCircle; shape < ShapeCount; shape++) {
        uint16_t factor = shape_speed_factor(shape, kind);
        if (factor > max_factor) {
            max_factor = factor;
        }
    }
    return max_factor;
}

static void test_each_terrain_has_one_strict_best(void) {
    for (TerrainKind kind = TerrainFlat; kind < TerrainKindCount; kind++) {
        uint16_t max_factor = max_factor_for(kind);
        assert(count_maximal_shapes(kind, max_factor) == 1);
    }
}

static void test_best_shape_matches_design(void) {
    assert(shape_best_for(TerrainFlat) == ShapeCircle);
    assert(shape_best_for(TerrainRocky) == ShapeLine);
    assert(shape_best_for(TerrainObstacle) == ShapeLine);

    ShapeId uphill_best = shape_best_for(TerrainUphill);
    assert(uphill_best == ShapeSquare || uphill_best == ShapeTriangle);
}

static void test_uphill_square_and_triangle_both_strong(void) {
    uint16_t square = shape_speed_factor(ShapeSquare, TerrainUphill);
    uint16_t triangle = shape_speed_factor(ShapeTriangle, TerrainUphill);
    uint16_t circle = shape_speed_factor(ShapeCircle, TerrainUphill);
    uint16_t line = shape_speed_factor(ShapeLine, TerrainUphill);

    // Both grippy shapes clearly beat the two non-grippy shapes on an uphill.
    assert(square > circle && square > line);
    assert(triangle > circle && triangle > line);
}

static void test_obstacle_only_line_passes(void) {
    uint16_t line = shape_speed_factor(ShapeLine, TerrainObstacle);
    uint16_t circle = shape_speed_factor(ShapeCircle, TerrainObstacle);
    uint16_t square = shape_speed_factor(ShapeSquare, TerrainObstacle);
    uint16_t triangle = shape_speed_factor(ShapeTriangle, TerrainObstacle);

    assert(line >= STALL_THRESHOLD);
    assert(circle < STALL_THRESHOLD);
    assert(square < STALL_THRESHOLD);
    assert(triangle < STALL_THRESHOLD);
}

int main(void) {
    test_every_factor_in_range();
    printf("test_every_factor_in_range: PASS\n");

    test_each_terrain_has_one_strict_best();
    printf("test_each_terrain_has_one_strict_best: PASS\n");

    test_best_shape_matches_design();
    printf("test_best_shape_matches_design: PASS\n");

    test_uphill_square_and_triangle_both_strong();
    printf("test_uphill_square_and_triangle_both_strong: PASS\n");

    test_obstacle_only_line_passes();
    printf("test_obstacle_only_line_passes: PASS\n");

    return 0;
}
