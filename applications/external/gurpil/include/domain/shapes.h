#pragma once

#include "include/domain/terrain_kind.h"

#include <stdint.h>

/* The wheel shapes the player can pick with the D-pad. Pure domain enum: no furi, no I/O. */
typedef enum {
    ShapeCircle,
    ShapeLine,
    ShapeSquare,
    ShapeTriangle,
    ShapeCount,
} ShapeId;

/* Fixed-point speed factor on a 0..256 scale (256 == 1.0x). This is the single source of
 * truth for how a wheel shape performs on a terrain kind — the "stable-hybrid" arcade model:
 * no real physics, just a tuned lookup table encoding the intended design:
 *   - flat:     circle is best.
 *   - rocky:    line is best.
 *   - uphill:   square and triangle are both strong/grippy; triangle edges out square.
 *   - obstacle: line passes through; the other three shapes stall.
 */
uint16_t shape_speed_factor(ShapeId shape, TerrainKind kind);

/* Returns the shape with the strictly-greatest speed factor for `kind` — the shape the
 * design intends the player to pick on that terrain. */
ShapeId shape_best_for(TerrainKind kind);
