#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_find_path_step_current(
    FrGame* game,
    const FrActor* actor,
    uint8_t target_x,
    uint8_t target_y,
    int8_t* out_dx,
    int8_t* out_dy);
bool fr_random_reachable_tile(FrGame* game, uint8_t* x, uint8_t* y, uint8_t steps);
bool fr_random_reachable_tile_reveal(FrGame* game, uint8_t* x, uint8_t* y, uint8_t steps);
bool fr_phase_shift_tile_reveal(FrGame* game, uint8_t* x, uint8_t* y, uint8_t steps);
