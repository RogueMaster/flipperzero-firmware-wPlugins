// Battle Definitions
#pragma once

#include <gui/gui.h>

typedef struct Battle Battle;

// Battle functions
Battle* battle_create_with_selection(int player_index, int enemy_index);
void battle_free(Battle* battle);
void battle_draw(Battle* battle, Canvas* canvas);
void battle_handle_input(Battle* battle, InputKey key);
bool battle_is_over(Battle* battle);
