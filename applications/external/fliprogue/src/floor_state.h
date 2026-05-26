#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

void fr_saved_actor_from_actor(FrSavedActor* saved, const FrActor* actor);
void fr_actor_from_saved_actor(FrActor* actor, const FrSavedActor* saved);
void fr_saved_item_from_item(FrSavedItem* saved, const FrItem* item);
void fr_item_from_saved_item(FrItem* item, const FrSavedItem* saved);
void fr_save_current_floor(FrGame* game);
bool fr_floor_actor_at_except(const FrFloorState* floor, uint8_t x, uint8_t y, uint8_t except_index);
void fr_init_floor_state(FrGame* game, uint8_t floor_id, uint8_t up_x, uint8_t up_y, uint8_t down_x, uint8_t down_y);
void fr_apply_saved_floor_state(FrGame* game, uint8_t floor_id);
void fr_enter_floor(FrGame* game, uint8_t floor, uint8_t entry_stairs);
