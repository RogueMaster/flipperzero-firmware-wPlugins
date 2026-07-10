#include "include/domain/shapes.h"

// Named tiers for the tuning table below — no bare magic numbers. Scale is 0..256, where
// 256 == 1.0x (full speed) and 0 == fully stopped. A plain enum (not `static const`) so
// each value is a true integer constant expression, usable in the static initializer below.
enum {
    FACTOR_BEST = 256,   // the ideal shape for this terrain
    FACTOR_STRONG = 224, // near-best; a second shape that is also strong
    FACTOR_GOOD = 192,   // decent, workable performance
    FACTOR_POOR = 96,    // struggles noticeably
    FACTOR_STALL = 16,   // effectively stopped (obstacle collision)
};

// TABLE[shape][kind]. Rows follow ShapeId order, columns follow TerrainKind order.
static const uint16_t TABLE[ShapeCount][TerrainKindCount] = {
    // Flat,          Rocky,          Uphill,          Obstacle
    [ShapeCircle] = {FACTOR_BEST, FACTOR_POOR, FACTOR_POOR, FACTOR_STALL},
    [ShapeLine] = {FACTOR_GOOD, FACTOR_BEST, FACTOR_POOR, FACTOR_GOOD},
    [ShapeSquare] = {FACTOR_POOR, FACTOR_GOOD, FACTOR_STRONG, FACTOR_STALL},
    [ShapeTriangle] = {FACTOR_POOR, FACTOR_GOOD, FACTOR_BEST, FACTOR_STALL},
};

uint16_t shape_speed_factor(ShapeId shape, TerrainKind kind) {
    return TABLE[shape][kind];
}

ShapeId shape_best_for(TerrainKind kind) {
    ShapeId best = ShapeCircle;
    uint16_t best_factor = TABLE[ShapeCircle][kind];
    for (ShapeId shape = ShapeCircle + 1; shape < ShapeCount; shape++) {
        uint16_t factor = TABLE[shape][kind];
        if (factor > best_factor) {
            best_factor = factor;
            best = shape;
        }
    }
    return best;
}
