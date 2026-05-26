#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

FrActor* fr_line_actor(FrGame* game, int8_t dx, int8_t dy);
bool fr_fear_direction(const FrGame* game, int8_t* dx, int8_t* dy);
bool fr_resolve_blink_destination(FrGame* game, uint8_t tx, uint8_t ty, uint8_t* out_x, uint8_t* out_y);
