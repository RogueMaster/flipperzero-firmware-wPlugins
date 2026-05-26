#pragma once

#include "game_logic.h"
#include "room_templates.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_item_exists_at(const FrGame* game, uint8_t x, uint8_t y);
bool fr_blocking_item_at(const FrGame* game, uint8_t x, uint8_t y);
bool fr_can_place_object(FrGame* game, uint8_t x, uint8_t y);
bool fr_find_free_near(FrGame* game, uint8_t* x, uint8_t* y);
bool fr_find_floor_near(FrGame* game, uint8_t* x, uint8_t* y, uint8_t max_radius);
bool fr_room_is_large_enough(const FrRoom* room, uint8_t min_w, uint8_t min_h);
bool fr_find_room_center_floor(FrGame* game, const FrRoom* room, uint8_t* x, uint8_t* y);
bool fr_find_room_perimeter_floor(FrGame* game, const FrRoom* room, uint8_t* x, uint8_t* y);
bool fr_find_wall_opposed_floor_in_room(
    FrGame* game,
    const FrRoom* room,
    uint8_t* trigger_x,
    uint8_t* trigger_y,
    uint8_t* wall_x,
    uint8_t* wall_y);
bool fr_find_secret_wall_candidate_for_room(FrGame* game, const FrRoom* room, uint8_t* x, uint8_t* y);
