#include "include/domain/terrain.h"

enum {
    // Zone length range, in x units of game distance. A "zone" is a contiguous run of one
    // terrain kind before the next zone begins.
    ZONE_LENGTH_MIN = 20,
    ZONE_LENGTH_MAX = 60,
    ZONE_LENGTH_SPAN = ZONE_LENGTH_MAX - ZONE_LENGTH_MIN + 1,

    // The very first zone is always flat, so every run starts gentle; the ramp weights (mostly
    // flat/rocky at low progress, see EASY_WEIGHT_* below) take over from the second zone, so
    // shape choice (circle vs line) already matters within the first stretch rather than only
    // far out.
    OPENING_ZONE_COUNT = 1,

    // Difficulty ramps linearly from the "easy" kind weights (x = 0) to the "hard" weights
    // (x >= RAMP_DISTANCE_UNITS); beyond that distance the hard weights hold steady.
    RAMP_DISTANCE_UNITS = 2000,
    RAMP_PROGRESS_MAX = 100,

    // Weighted-random kind picker, at the two ends of the ramp. Order: flat, rocky, uphill,
    // obstacle. Only the ratios matter, and both ends must stay strictly positive for every
    // kind so the picker always has a non-empty weight to fall back on.
    EASY_WEIGHT_FLAT = 60,
    EASY_WEIGHT_ROCKY = 30,
    EASY_WEIGHT_UPHILL = 8,
    EASY_WEIGHT_OBSTACLE = 2,
    HARD_WEIGHT_FLAT = 15,
    HARD_WEIGHT_ROCKY = 25,
    HARD_WEIGHT_UPHILL = 30,
    HARD_WEIGHT_OBSTACLE = 30,

    // Height shaping, in the game-unit band documented in terrain.h (TERRAIN_HEIGHT_MIN..MAX).
    HEIGHT_BASELINE = 20, // flat surface height
    ROCKY_AMPLITUDE = 8,  // rocky bumps swing +/- this around the baseline
    UPHILL_MAX_RISE = 16, // uphill rises from baseline to baseline + this across the zone
    OBSTACLE_BUMP = 4,    // obstacle marker: a modest raise, not a wall — the stall is gameplay
};

// Large odd multiplier (Knuth's multiplicative hash) used to spread zone indices across the
// 32-bit state space, so consecutive zone indices don't seed the PRNG with near-identical
// values.
static const uint32_t ZONE_SEED_MULTIPLIER = 2654435761u;

uint32_t gurpil_rng_seed(uint32_t seed) {
    // xorshift32 is stuck at zero forever if seeded with zero; nudge it to a fixed non-zero
    // value so every caller-supplied seed, including 0, produces a valid sequence.
    return seed == 0 ? 1u : seed;
}

uint32_t gurpil_rng_next(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// Per-zone RNG state: a pure function of (seed, zone_index), independent of any other zone's
// draws. This is what lets `terrain_at` recompute any zone on demand instead of needing to
// remember state across calls.
static uint32_t zone_rng_state(uint32_t seed, int32_t zone_index) {
    uint32_t mixed = seed ^ ((uint32_t)zone_index * ZONE_SEED_MULTIPLIER);
    return gurpil_rng_seed(mixed);
}

static int32_t zone_length(uint32_t *rng_state) {
    return ZONE_LENGTH_MIN + (int32_t)(gurpil_rng_next(rng_state) % ZONE_LENGTH_SPAN);
}

// Linear interpolation between a kind's easy/hard weight, at ramp progress
// `progress_percent` (0 = opening, RAMP_PROGRESS_MAX = fully ramped).
static int32_t ramped_weight(int32_t easy, int32_t hard, int32_t progress_percent) {
    return easy + ((hard - easy) * progress_percent) / RAMP_PROGRESS_MAX;
}

static int32_t ramp_progress_percent(int32_t zone_start_x) {
    if (zone_start_x >= RAMP_DISTANCE_UNITS) {
        return RAMP_PROGRESS_MAX;
    }
    return (zone_start_x * RAMP_PROGRESS_MAX) / RAMP_DISTANCE_UNITS;
}

static TerrainKind pick_kind(uint32_t *rng_state, int32_t zone_start_x) {
    int32_t progress = ramp_progress_percent(zone_start_x);
    int32_t weight_flat = ramped_weight(EASY_WEIGHT_FLAT, HARD_WEIGHT_FLAT, progress);
    int32_t weight_rocky = ramped_weight(EASY_WEIGHT_ROCKY, HARD_WEIGHT_ROCKY, progress);
    int32_t weight_uphill = ramped_weight(EASY_WEIGHT_UPHILL, HARD_WEIGHT_UPHILL, progress);
    int32_t weight_obstacle = ramped_weight(EASY_WEIGHT_OBSTACLE, HARD_WEIGHT_OBSTACLE, progress);
    int32_t total = weight_flat + weight_rocky + weight_uphill + weight_obstacle;

    int32_t draw = (int32_t)(gurpil_rng_next(rng_state) % (uint32_t)total);
    if (draw < weight_flat) {
        return TerrainFlat;
    }
    draw -= weight_flat;
    if (draw < weight_rocky) {
        return TerrainRocky;
    }
    draw -= weight_rocky;
    if (draw < weight_uphill) {
        return TerrainUphill;
    }
    return TerrainObstacle;
}

// Cheap deterministic hash of (seed_value, offset). Used to shape rocky bumps per-x without
// needing a sequential RNG walk across every x inside the zone — each x is derived
// independently and purely from the zone's own seed.
static uint32_t hash_mix(uint32_t seed_value, int32_t offset) {
    uint32_t state = gurpil_rng_seed(seed_value ^ (uint32_t)offset);
    gurpil_rng_next(&state);
    return state;
}

static int16_t clamp_height(int32_t height) {
    if (height < TERRAIN_HEIGHT_MIN) {
        return TERRAIN_HEIGHT_MIN;
    }
    if (height > TERRAIN_HEIGHT_MAX) {
        return TERRAIN_HEIGHT_MAX;
    }
    return (int16_t)height;
}

static int16_t height_for(TerrainKind kind, uint32_t zone_seed_value, int32_t offset_in_zone,
                          int32_t length) {
    switch (kind) {
        case TerrainRocky: {
            uint32_t hash = hash_mix(zone_seed_value, offset_in_zone);
            int32_t bump = (int32_t)(hash % (2 * ROCKY_AMPLITUDE + 1)) - ROCKY_AMPLITUDE;
            return clamp_height(HEIGHT_BASELINE + bump);
        }
        case TerrainUphill: {
            int32_t rise = length > 0 ? (offset_in_zone * UPHILL_MAX_RISE) / length : 0;
            return clamp_height(HEIGHT_BASELINE + rise);
        }
        case TerrainObstacle:
            return clamp_height(HEIGHT_BASELINE + OBSTACLE_BUMP);
        default: // TerrainFlat, and the TerrainKindCount sentinel as a defensive fallback.
            return clamp_height(HEIGHT_BASELINE);
    }
}

TerrainSample terrain_at(uint32_t seed, int32_t x) {
    if (x < 0) {
        x = 0;
    }

    int32_t zone_index = 0;
    int32_t zone_start = 0;
    for (;;) {
        uint32_t rng_state = zone_rng_state(seed, zone_index);
        int32_t length = zone_length(&rng_state);

        if (x < zone_start + length) {
            TerrainKind kind =
                (zone_index < OPENING_ZONE_COUNT) ? TerrainFlat : pick_kind(&rng_state, zone_start);
            int32_t offset_in_zone = x - zone_start;
            TerrainSample sample = {kind, height_for(kind, rng_state, offset_in_zone, length)};
            return sample;
        }

        zone_start += length;
        zone_index++;
    }
}
