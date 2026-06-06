#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

void fr_freeze_water_area(FrGame* game, uint8_t floor, uint8_t cx, uint8_t cy, uint8_t radius);
void fr_tick_ice_field(FrGame* game, FrTerrainField* field);
bool fr_try_ice_slide_player(FrGame* game, int8_t dx, int8_t dy);
bool fr_try_ice_slide_actor(FrGame* game, FrActor* actor, int8_t dx, int8_t dy);
