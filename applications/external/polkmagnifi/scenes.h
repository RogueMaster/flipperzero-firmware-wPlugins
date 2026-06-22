#pragma once

#include <gui/scene_manager.h>

typedef enum {
    SceneMain,
    SceneSubMenu,
    SceneCount,
} SceneId;

// Custom event fired by main view input callback when a submenu row is selected
#define PolkEventOpenSubmenu 0

// ── Scene callbacks ───────────────────────────────────────────────────────────

void scene_main_on_enter(void* ctx);
bool scene_main_on_event(void* ctx, SceneManagerEvent event);
void scene_main_on_exit(void* ctx);

void scene_submenu_on_enter(void* ctx);
bool scene_submenu_on_event(void* ctx, SceneManagerEvent event);
void scene_submenu_on_exit(void* ctx);
