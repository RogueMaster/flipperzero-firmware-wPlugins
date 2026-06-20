#include "special_floors.h"

static uint32_t fr_special_mix(uint32_t run_seed, uint8_t floor) {
    uint32_t x = run_seed ? run_seed : 1u;
    x ^= (uint32_t)floor * 0xA511E9B3u;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return x;
}

uint8_t fr_special_floor_type(uint32_t run_seed, uint8_t floor) {
    if(floor < 4 || floor > 17) return FR_SPECIAL_FLOOR_NONE;
    uint8_t roll = (uint8_t)(fr_special_mix(run_seed, floor) % 18u);
    if(roll == 0) return FR_SPECIAL_FLOOR_FLOODED;
    if(roll == 1) return FR_SPECIAL_FLOOR_TRAPWORKS;
    if(roll == 2) return FR_SPECIAL_FLOOR_SHRINE;
    if(roll == 3) return FR_SPECIAL_FLOOR_NEST;
    if(roll == 4) return FR_SPECIAL_FLOOR_MAZE;
    return FR_SPECIAL_FLOOR_NONE;
}
