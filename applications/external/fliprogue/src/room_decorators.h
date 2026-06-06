#pragma once

#include "game_logic.h"
#include "room_templates.h"

typedef enum {
    FR_ROOM_DECOR_NONE = 0,
    FR_ROOM_DECOR_GRASS_LURKER_CANDIDATE,
    FR_ROOM_DECOR_CHEST_TREASURE,
    FR_ROOM_DECOR_SHRINE,
    FR_ROOM_DECOR_FLOODED_POOL,
    FR_ROOM_DECOR_TRAP_ROOM,
    FR_ROOM_DECOR_AMBUSH_PACK,
    FR_ROOM_DECOR_SECRET_REWARD,
} FrRoomDecorator;

void fr_apply_room_decorators(
    FrGame* game,
    const FrRoom* rooms,
    uint8_t room_count,
    uint8_t floor,
    uint8_t template_id,
    uint8_t special_type);
