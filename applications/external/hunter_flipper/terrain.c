#include "terrain.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Diamond-square algorithm constants
#define MAX_DELTA       0.3f
#define ROUGHNESS_DECAY 2.0f

// Random number generation with seed
static uint32_t terrain_seed = 12345;

static void terrain_srand(uint32_t seed) {
    terrain_seed = seed;
}

static float terrain_rand(void) {
    terrain_seed = terrain_seed * 1103515245 + 12345;
    return ((terrain_seed >> 16) & 0x7fff) / (float)0x7fff;
}

static float terrain_rand_range(float range) {
    return terrain_rand() * range - (range / 2.0f);
}

TerrainManager* terrain_manager_alloc(uint32_t seed, float elevation) {
    TerrainManager* terrain = malloc(sizeof(TerrainManager));
    if(!terrain) return NULL;

    terrain->width = MAX_TERRAIN_SIZE;
    terrain->height = MAX_TERRAIN_SIZE;
    terrain->elevation_threshold = elevation;
    terrain->seed = seed;

    // Allocate height map
    size_t map_size = terrain->width * terrain->height;
    terrain->height_map = malloc(map_size * sizeof(float));
    terrain->collision_map = malloc(map_size * sizeof(bool));

    if(!terrain->height_map || !terrain->collision_map) {
        terrain_manager_free(terrain);
        return NULL;
    }

    // Generate terrain
    terrain_generate_diamond_square(terrain);
    terrain_apply_elevation_threshold(terrain);

    return terrain;
}

void terrain_manager_free(TerrainManager* terrain) {
    if(!terrain) return;

    if(terrain->height_map) free(terrain->height_map);
    if(terrain->collision_map) free(terrain->collision_map);
    free(terrain);
}

static float terrain_get_height(TerrainManager* terrain, int x, int y) {
    if(x < 0 || x >= terrain->width || y < 0 || y >= terrain->height) {
        return 0.0f;
    }
    return terrain->height_map[y * terrain->width + x];
}

static void terrain_set_height(TerrainManager* terrain, int x, int y, float height) {
    if(x < 0 || x >= terrain->width || y < 0 || y >= terrain->height) {
        return;
    }
    terrain->height_map[y * terrain->width + x] = height;
}

static void terrain_init_corners(TerrainManager* terrain) {
    int step = TERRAIN_SIZE - 1;
    terrain_srand(terrain->seed);

    // Initialize corner values for each chunk
    for(int chunk_x = 0; chunk_x < TERRAIN_CHUNKS; chunk_x++) {
        for(int chunk_y = 0; chunk_y < TERRAIN_CHUNKS; chunk_y++) {
            int base_x = chunk_x * step;
            int base_y = chunk_y * step;

            terrain_srand(terrain->seed + (base_x * base_y * 6));
            terrain_set_height(terrain, base_x, base_y, terrain_rand());
        }
    }
}

static void
    terrain_diamond_step(TerrainManager* terrain, int x, int y, int size, float roughness) {
    int half = size / 2;

    // Get corner values
    float tl = terrain_get_height(terrain, x - half, y - half);
    float tr = terrain_get_height(terrain, x + half, y - half);
    float bl = terrain_get_height(terrain, x - half, y + half);
    float br = terrain_get_height(terrain, x + half, y + half);

    // Calculate average and add random offset
    float avg = (tl + tr + bl + br) / 4.0f;
    float offset = terrain_rand_range(roughness);

    terrain_set_height(terrain, x, y, avg + offset);
}

static void terrain_square_step(TerrainManager* terrain, int x, int y, int size, float roughness) {
    int half = size / 2;
    float total = 0.0f;
    int count = 0;

    // Sample neighboring points
    if(x - half >= 0) {
        total += terrain_get_height(terrain, x - half, y);
        count++;
    }
    if(x + half < terrain->width) {
        total += terrain_get_height(terrain, x + half, y);
        count++;
    }
    if(y - half >= 0) {
        total += terrain_get_height(terrain, x, y - half);
        count++;
    }
    if(y + half < terrain->height) {
        total += terrain_get_height(terrain, x, y + half);
        count++;
    }

    if(count > 0) {
        float avg = total / count;
        float offset = terrain_rand_range(roughness);
        terrain_set_height(terrain, x, y, avg + offset);
    }
}

void terrain_generate_diamond_square(TerrainManager* terrain) {
    // Initialize corners
    terrain_init_corners(terrain);

    int size = TERRAIN_SIZE;
    float roughness = MAX_DELTA;

    while(size >= 3) {
        int half = size / 2;

        // Diamond step
        for(int y = half; y < terrain->height; y += size - 1) {
            for(int x = half; x < terrain->width; x += size - 1) {
                terrain_diamond_step(terrain, x, y, size, roughness);
            }
        }

        // Square step
        for(int y = 0; y < terrain->height; y += half) {
            for(int x = (y / half) % 2 == 0 ? half : 0; x < terrain->width; x += size - 1) {
                terrain_square_step(terrain, x, y, size, roughness);
            }
        }

        size = half + 1;
        roughness /= ROUGHNESS_DECAY;
    }
}

void terrain_apply_elevation_threshold(TerrainManager* terrain) {
    for(int y = 0; y < terrain->height; y++) {
        for(int x = 0; x < terrain->width; x++) {
            int idx = y * terrain->width + x;
            float height = terrain->height_map[idx];
            terrain->collision_map[idx] = (height > terrain->elevation_threshold);
        }
    }

    // Apply despeckle filter - remove isolated land pixels
    bool* temp_map = malloc(terrain->width * terrain->height * sizeof(bool));
    if(!temp_map) return;

    memcpy(temp_map, terrain->collision_map, terrain->width * terrain->height * sizeof(bool));

    for(int y = 0; y < terrain->height; y++) {
        for(int x = 0; x < terrain->width; x++) {
            int idx = y * terrain->width + x;
            if(temp_map[idx]) { // If this is land
                bool has_neighbor = false;

                // Check 4-connected neighbors
                for(int dy = -1; dy <= 1; dy++) {
                    for(int dx = -1; dx <= 1; dx++) {
                        if(dx == 0 && dy == 0) continue;
                        if(abs(dx) + abs(dy) > 1) continue; // Only 4-connected

                        int nx = x + dx;
                        int ny = y + dy;

                        if(nx >= 0 && nx < terrain->width && ny >= 0 && ny < terrain->height) {
                            if(temp_map[ny * terrain->width + nx]) {
                                has_neighbor = true;
                                break;
                            }
                        }
                    }
                    if(has_neighbor) break;
                }

                if(!has_neighbor) {
                    terrain->collision_map[idx] = false; // Remove isolated pixel
                }
            }
        }
    }

    free(temp_map);
}

bool terrain_check_collision(TerrainManager* terrain, int x, int y) {
    if(!terrain || x < 0 || x >= terrain->width || y < 0 || y >= terrain->height) {
        return false;
    }
    return terrain->collision_map[y * terrain->width + x];
}

void terrain_render_area(
    TerrainManager* terrain,
    Canvas* canvas,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    if(!terrain) return;

    // Clamp to screen and terrain bounds
    start_x = MAX(0, MIN(start_x, terrain->width - 1));
    start_y = MAX(0, MIN(start_y, terrain->height - 1));
    end_x = MAX(0, MIN(end_x, terrain->width - 1));
    end_y = MAX(0, MIN(end_y, terrain->height - 1));

    // Render terrain pixels
    for(int y = start_y; y <= end_y; y++) {
        for(int x = start_x; x <= end_x; x++) {
            if(terrain_check_collision(terrain, x, y)) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}
