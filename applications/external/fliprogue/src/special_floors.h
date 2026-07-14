#pragma once

#include <stdint.h>

typedef enum {
    FR_SPECIAL_FLOOR_NONE = 0,
    FR_SPECIAL_FLOOR_FLOODED = 1,
    FR_SPECIAL_FLOOR_TRAPWORKS = 2,
    FR_SPECIAL_FLOOR_SHRINE = 3,
    FR_SPECIAL_FLOOR_NEST = 4,
    FR_SPECIAL_FLOOR_MAZE = 5,
    FR_SPECIAL_FLOOR_MAX = 6,
} FrSpecialFloorType;

uint8_t fr_special_floor_type(uint32_t run_seed, uint8_t floor);
