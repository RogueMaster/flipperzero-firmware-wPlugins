/**
 * @file flippass_scene_status.h
 * @brief Status and error scene for the FlipPass application.
 *
 * This scene presents short loading, error, or empty-state messages using the
 * shared widget view.
 */
#pragma once

#include <gui/scene_manager.h>

struct App;

void flippass_scene_status_show(
    struct App* app,
    const char* title,
    const char* body,
    uint32_t return_scene);
void flippass_scene_status_on_enter(void* context);
bool flippass_scene_status_on_event(void* context, SceneManagerEvent event);
void flippass_scene_status_on_exit(void* context);
