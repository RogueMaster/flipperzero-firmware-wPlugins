#pragma once

#include <gui/scene_manager.h>

/* ── Scene ID enum ────────────────────────────────────────────────────── */
#define ADD_SCENE(prefix, name, id) FasScene##id,
typedef enum {
#include "fas_scene_config.h"
    FasSceneCount,
} FasScene;
#undef ADD_SCENE

/* ── Handler table (defined in fas_scene.c) ───────────────────────────── */
extern const SceneManagerHandlers fas_scene_handlers;

/* ── Per-scene on_enter / on_event / on_exit declarations ──────────────── */
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "fas_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void*, SceneManagerEvent);
#include "fas_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void*);
#include "fas_scene_config.h"
#undef ADD_SCENE
