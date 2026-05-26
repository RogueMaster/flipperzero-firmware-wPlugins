#pragma once

#include "game_logic.h"

#define FR_ROOM_MAX 7
#define FR_ROOM_TEMPLATE_COUNT 5
#define FR_ROOM_TEMPLATE_MAZE 4

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
} FrRoom;

uint8_t fr_room_template_id(uint32_t run_seed, uint8_t floor);
void fr_room_template_build(
    FrGame* game,
    uint8_t floor,
    FrRoom rooms[FR_ROOM_MAX],
    uint8_t* room_count,
    uint8_t* template_id);
