#pragma once

#include <gui/scene_manager.h>
#include "../fire_string.h"

void fire_string_scene_on_enter_string_generator(void* context);
bool fire_string_scene_on_event_string_generator(void* context, SceneManagerEvent event);
void fire_string_scene_on_exit_string_generator(void* context);

const char* get_rnd_word(FireString* app, bool save);
char get_rnd_char(FireString* app, bool save);
