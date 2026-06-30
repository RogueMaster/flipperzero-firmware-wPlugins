#pragma once
#include "constants.h"
#include "player.h"
#include "obstacles.h"

// Function prototypes for game logic
void process_game_logic(GeometryDashApp* app, uint32_t dt_ms);
bool check_collision(GeometryDashApp* app, float scroll_offset);