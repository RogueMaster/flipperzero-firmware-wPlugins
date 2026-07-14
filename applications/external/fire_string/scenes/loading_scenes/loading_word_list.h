#pragma once

#include <gui/scene_manager.h>

void fire_string_scene_on_enter_loading_word_list(void* context);
bool fire_string_scene_on_event_loading_word_list(void* context, SceneManagerEvent event);
void fire_string_scene_on_exit_loading_word_list(void* context);
