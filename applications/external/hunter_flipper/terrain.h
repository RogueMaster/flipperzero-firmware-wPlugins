#pragma once
#include "engine/engine.h"

// Terrain configuration - optimized for Flipper Zero memory
#define TERRAIN_SIZE     65 // 2^6 + 1 for diamond-square
#define TERRAIN_CHUNKS   2 // Reduced from 4 for memory
#define MAX_TERRAIN_SIZE ((TERRAIN_SIZE - 1) * TERRAIN_CHUNKS)

typedef struct {
    float* height_map;
    bool* collision_map;
    uint16_t width;
    uint16_t height;
    float elevation_threshold;
    uint32_t seed;
} TerrainManager;

// Terrain generation functions
TerrainManager* terrain_manager_alloc(uint32_t seed, float elevation);
void terrain_manager_free(TerrainManager* terrain);
bool terrain_check_collision(TerrainManager* terrain, int x, int y);
void terrain_render_area(
    TerrainManager* terrain,
    Canvas* canvas,
    int start_x,
    int start_y,
    int end_x,
    int end_y);

// Terrain generation utilities
void terrain_generate_diamond_square(TerrainManager* terrain);
void terrain_apply_elevation_threshold(TerrainManager* terrain);
