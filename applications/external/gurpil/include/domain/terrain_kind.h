#pragma once

// Shared single source of truth for terrain kinds. `domain/shapes` (Task 2) and
// `domain/terrain` (Task 3) both need this enum; it lives here on its own so neither
// module owns the other's concept.
typedef enum {
    TerrainFlat,
    TerrainRocky,
    TerrainUphill,
    TerrainObstacle,
    TerrainKindCount,
} TerrainKind;
