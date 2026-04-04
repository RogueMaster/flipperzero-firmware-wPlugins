#pragma once

#include <gui/scene_manager.h>

struct App;

void flippass_scene_text_view_show(
    struct App* app,
    const char* title,
    const char* body,
    uint32_t return_scene);
void flippass_scene_text_view_on_enter(void* context);
bool flippass_scene_text_view_on_event(void* context, SceneManagerEvent event);
void flippass_scene_text_view_on_exit(void* context);
