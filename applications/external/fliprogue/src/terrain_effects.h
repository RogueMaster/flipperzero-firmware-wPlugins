#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

void fr_ignite_fire_field(FrGame* game, uint8_t floor, uint8_t x, uint8_t y, uint8_t radius);
void fr_refresh_or_expand_fire_field(FrGame* game, uint8_t floor, uint8_t x, uint8_t y, uint8_t base_radius);
bool fr_fire_area_has_fire(const FrGame* game, uint8_t floor, uint8_t cx, uint8_t cy, uint8_t radius);
bool fr_extinguish_fire_area(FrGame* game, uint8_t floor, uint8_t cx, uint8_t cy, uint8_t radius);
void fr_tick_terrain_effects(FrGame* game);
bool fr_terrain_fire_at(const FrGame* game, uint8_t floor, uint8_t x, uint8_t y);
uint8_t fr_active_terrain_field_count(const FrGame* game);
FrTerrainField* fr_alloc_terrain_field(FrGame* game);
bool fr_terrain_field_cell_get(const FrTerrainField* field, uint8_t x, uint8_t y);
void fr_terrain_field_cell_set(FrTerrainField* field, uint8_t x, uint8_t y);
void fr_terrain_field_cell_clear(FrTerrainField* field, uint8_t x, uint8_t y);
bool fr_terrain_field_has_any_cell(const FrTerrainField* field);
