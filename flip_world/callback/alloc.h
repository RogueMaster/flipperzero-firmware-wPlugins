#pragma once
#include <flip_world.h>
#include <callback/utils.h>

bool alloc_message_view(void *context, MessageState state);
bool alloc_text_input_view(void *context, char *title);
bool alloc_variable_item_list(void *context, uint32_t view_id);
bool alloc_submenu_other(void *context, uint32_t view_id);
bool alloc_game_submenu(void *context);