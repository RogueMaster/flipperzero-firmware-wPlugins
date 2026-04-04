/**
 * @file flippass_scene_entry_detail.h
 * @brief Entry detail scene for the FlipPass application.
 *
 * This scene renders a Flipper-friendly detail card for the currently
 * selected KeePass entry.
 */
#pragma once

#include <gui/scene_manager.h>

struct App;

void flippass_scene_entry_detail_on_enter(void* context);
bool flippass_scene_entry_detail_on_event(void* context, SceneManagerEvent event);
void flippass_scene_entry_detail_on_exit(void* context);
