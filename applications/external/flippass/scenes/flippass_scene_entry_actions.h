/**
 * @file flippass_scene_entry_actions.h
 * @brief Entry action menu scene for the FlipPass application.
 */
#pragma once

#include <furi.h>
#include <gui/scene_manager.h>

struct App;

void flippass_scene_entry_actions_on_enter(void* context);
bool flippass_scene_entry_actions_on_event(void* context, SceneManagerEvent event);
void flippass_scene_entry_actions_on_exit(void* context);
bool flippass_entry_actions_trigger(struct App* app, uint32_t index, FuriString* error);
