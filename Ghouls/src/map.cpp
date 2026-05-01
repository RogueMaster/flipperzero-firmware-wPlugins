#include "map.hpp"

#ifdef ENGINE_STORAGE_INCLUDE
#include ENGINE_STORAGE_INCLUDE
#endif

bool mapPackLoadFromFile(const char *filename, map_data_t *outMapData)
{
#if defined(ENGINE_STORAGE_INCLUDE) && defined(ENGINE_STORAGE_READ)
    const size_t expectedSize = sizeof(map_data_t);
    return ENGINE_STORAGE_READ(filename, outMapData, expectedSize) == expectedSize;
#else
    (void)filename;
    (void)outMapData;
    return false;
#endif
}

bool mapPackSaveToFile(const char *filename, const map_data_t *mapData)
{
#if defined(ENGINE_STORAGE_INCLUDE) && defined(ENGINE_STORAGE_WRITE)
    return ENGINE_STORAGE_WRITE(filename, mapData, sizeof(map_data_t));
#else
    (void)filename;
    (void)mapData;
    return false;
#endif
}

const map_data_t defaultMapPack = {
    // map data
    .width = 96,
    .height = 48,
    .skyDayGradient = {180, 220, 255, 100, 160, 255}, // sunny
    .skyNightGradient = {40, 50, 120, 10, 15, 50},    // dark
    .groundDayGradient = {200, 140, 70, 140, 90, 40}, // dirt
    .groundNightGradient = {60, 45, 25, 22, 16, 8},   // dark
    .wallColor = 0x0000,
    .vWallCount = 12,
    .vWallPositions = {
        // left wall (x=0.5)
        Vector(0.5f, 4.0f),
        Vector(0.5f, 12.0f),
        Vector(0.5f, 20.0f),
        Vector(0.5f, 28.0f),
        Vector(0.5f, 36.0f),
        Vector(0.5f, 44.0f),
        // right wall (x=95.5)
        Vector(95.5f, 4.0f),
        Vector(95.5f, 12.0f),
        Vector(95.5f, 20.0f),
        Vector(95.5f, 28.0f),
        Vector(95.5f, 36.0f),
        Vector(95.5f, 44.0f),
    },

    .hWallCount = 24,
    .hWallPositions = {
        // top wall (y=0.5)
        Vector(4.0f, 0.5f),
        Vector(12.0f, 0.5f),
        Vector(20.0f, 0.5f),
        Vector(28.0f, 0.5f),
        Vector(36.0f, 0.5f),
        Vector(44.0f, 0.5f),
        Vector(52.0f, 0.5f),
        Vector(60.0f, 0.5f),
        Vector(68.0f, 0.5f),
        Vector(76.0f, 0.5f),
        Vector(84.0f, 0.5f),
        Vector(92.0f, 0.5f),
        // bottom wall (y=47.5)
        Vector(4.0f, 47.5f),
        Vector(12.0f, 47.5f),
        Vector(20.0f, 47.5f),
        Vector(28.0f, 47.5f),
        Vector(36.0f, 47.5f),
        Vector(44.0f, 47.5f),
        Vector(52.0f, 47.5f),
        Vector(60.0f, 47.5f),
        Vector(68.0f, 47.5f),
        Vector(76.0f, 47.5f),
        Vector(84.0f, 47.5f),
        Vector(92.0f, 47.5f),
    },

    // tree data
    .treeCount = 54,
    .treePositions = {
        Vector(8, 2),   //  0: (2,  0)
        Vector(8, 44),  //  1: (2, 11)
        Vector(12, 2),  //  2: (3,  0)
        Vector(12, 44), //  3: (3, 11)
        Vector(16, 2),  //  4: (4,  0)
        Vector(16, 44), //  5: (4, 11)
        Vector(20, 24), //  6: (5,  6)
        Vector(20, 28), //  7: (5,  7)
        Vector(28, 8),  //  8: (7,  2)
        Vector(28, 12), //  9: (7,  3)
        Vector(28, 16), // 10: (7,  4)
        Vector(32, 8),  // 11: (8,  2)
        Vector(32, 12), // 12: (8,  3)
        Vector(32, 16), // 13: (8,  4)
        Vector(36, 8),  // 14: (9,  2)
        Vector(40, 8),  // 15: (10, 2)
        Vector(40, 12), // 16: (10, 3)
        Vector(40, 16), // 17: (10, 4)
        Vector(44, 8),  // 18: (11, 2)
        Vector(44, 12), // 19: (11, 3)
        Vector(44, 16), // 20: (11, 4)
        Vector(32, 44), // 21: (8, 11)
        Vector(36, 44), // 22: (9, 11)
        Vector(40, 44), // 23: (10,11)
        Vector(44, 24), // 24: (11, 6)
        Vector(44, 28), // 25: (11, 7)
        Vector(48, 2),  // 26: (12, 0)
        Vector(52, 2),  // 27: (13, 0)
        Vector(56, 2),  // 28: (14, 0)
        Vector(64, 36), // 29: (16, 9)
        Vector(64, 44), // 30: (16,11)
        Vector(68, 36), // 31: (17, 9)
        Vector(68, 44), // 32: (17,11)
        Vector(72, 36), // 33: (18, 9)
        Vector(72, 44), // 34: (18,11)
        Vector(76, 2),  // 35: (19, 0)
        Vector(80, 2),  // 36: (20, 0)
        Vector(84, 2),  // 37: (21, 0)
        Vector(88, 2),  // 38: (22, 0)
        Vector(84, 36), // 39: (21, 9)
        Vector(88, 36), // 40: (22, 9)
        Vector(84, 44), // 41: (21,11)
        Vector(92, 2),  // 42: (23, 0)
        Vector(92, 4),  // 43: (23, 1)
        Vector(92, 8),  // 44: (23, 2)
        Vector(92, 12), // 45: (23, 3)
        Vector(92, 16), // 46: (23, 4)
        Vector(92, 20), // 47: (23, 5)
        Vector(92, 24), // 48: (23, 6)
        Vector(92, 28), // 49: (23, 7)
        Vector(92, 32), // 50: (23, 8)
        Vector(92, 36), // 51: (23, 9)
        Vector(92, 40), // 52: (23,10)
        Vector(92, 44), // 53: (23,11)
    },
    .treeColor = 0x13e2,

    // house data
    .houseCount = 6,
    .housePositions = {
        Vector(12, 8),  // 0
        Vector(12, 36), // 1
        Vector(36, 36), // 2
        Vector(72, 8),  // 3
        Vector(72, 28), // 4
        Vector(84, 28), // 5
    },
    .houseColor = 0xa0a1,

    // weapon data
    .weaponCount = 20,
    .weaponPositions = {
        Vector(8, 8),
        Vector(8, 36),
        Vector(12, 20),
        Vector(28, 24),
        Vector(32, 8),
        Vector(36, 12),
        Vector(52, 20),
        Vector(52, 44),
        Vector(60, 8),
        Vector(60, 12),
        Vector(60, 32),
        Vector(60, 36),
        Vector(64, 16),
        Vector(72, 16),
        Vector(80, 12),
        Vector(84, 16),
        Vector(88, 8),
        Vector(88, 16),
        Vector(88, 32),
        Vector(88, 44),
    },

    .ghoulCount = 20,
    .ghoulPositions = {
        Vector(60, 16),
        Vector(60, 44),
        Vector(64, 8),
        Vector(64, 12),
        Vector(64, 20),
        Vector(64, 24),
        Vector(64, 28),
        Vector(64, 32),
        Vector(68, 32),
        Vector(72, 32),
        Vector(76, 16),
        Vector(76, 40),
        Vector(80, 8),
        Vector(80, 16),
        Vector(80, 40),
        Vector(84, 12),
        Vector(84, 32),
        Vector(88, 8),
        Vector(88, 12),
        Vector(88, 40),
    }

};