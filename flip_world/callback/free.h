#pragma once
#include <flip_world.h>

void free_game_submenu(void *context);
void free_submenu_other(void *context);
void free_message_view(void *context);
void free_text_input_view(void *context);
void free_variable_item_list(void *context);
void free_all_views(void *context, bool free_variable_list, bool free_settings_other, bool free_submenu_game);