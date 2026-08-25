#pragma once

#include <gui/scene_manager.h>

void api_caller_scene_call_add_on_enter(void* context);
bool api_caller_scene_call_add_on_event(void* context, SceneManagerEvent event);
void api_caller_scene_call_add_on_exit(void* context);
