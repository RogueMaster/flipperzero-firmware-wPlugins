#pragma once
#include <gui/scene_manager.h>

typedef enum {
    PhrSceneMain,
    PhrSceneAlert,
    PhrSceneSettings,
    PhrSceneRule,
    PhrSceneCount,
} PhrScene;

extern const SceneManagerHandlers phr_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                  \
    void prefix##_scene_##name##_on_enter(void*);                    \
    bool prefix##_scene_##name##_on_event(void*, SceneManagerEvent); \
    void prefix##_scene_##name##_on_exit(void*);
ADD_SCENE(phr, main, Main)
ADD_SCENE(phr, alert, Alert)
ADD_SCENE(phr, settings, Settings)
ADD_SCENE(phr, rule, Rule)
#undef ADD_SCENE
