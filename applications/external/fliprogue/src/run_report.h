#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_boss_alive(FrGame* game);
bool fr_warden_position(const FrGame* game, uint8_t* floor, uint8_t* x, uint8_t* y);
const char* fr_run_summary(const FrGame* game);
const char* fr_death_log(const FrGame* game);
