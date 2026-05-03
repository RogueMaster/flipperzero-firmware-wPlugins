#pragma once
#include "pico-game-engine/engine/level.hpp"
#include "pico-game-engine/engine/sprite3d.hpp"
#include "dynamic_map.hpp"
#include "general.hpp"

#define MAX_TREE_COUNT 64
#define MAX_HOUSE_COUNT 32
#define MAX_WALL_COUNT 64
#define MAX_WEAPON_SPAWN 32 // only 4 weapons will spawn
#define MAX_GHOUL_SPAWN 32  // only ENEMY_SPAWN_MAX ghouls will spawn at a time

class GhoulsLevel;

typedef struct
{
    // map data
    uint8_t width;
    uint8_t height;
    gradient_color_t skyDayGradient;
    gradient_color_t skyNightGradient;
    gradient_color_t groundDayGradient;
    gradient_color_t groundNightGradient;
    uint16_t wallColor;
    uint8_t vWallCount;
    Vector vWallPositions[MAX_WALL_COUNT];
    uint8_t hWallCount;
    Vector hWallPositions[MAX_WALL_COUNT];

    // tree data
    uint8_t treeCount;
    Vector treePositions[MAX_TREE_COUNT];
    uint16_t treeColor;

    // house data
    uint8_t houseCount;
    Vector housePositions[MAX_HOUSE_COUNT];
    uint16_t houseColor;

    // weapon data
    uint8_t weaponCount;
    Vector weaponPositions[MAX_WEAPON_SPAWN];

    // ghoul data
    uint8_t ghoulCount;
    Vector ghoulPositions[MAX_GHOUL_SPAWN];
} map_data_t;

bool mapPackLoadFromFile(const char *filename, map_data_t *outMapData);
bool mapPackSaveToFile(const char *filename, const map_data_t *mapData);