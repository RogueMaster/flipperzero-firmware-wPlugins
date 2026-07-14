#pragma once

#include <gui/scene_manager.h>

typedef enum {
    SCENE_BROWSE,
    SCENE_SELECT,
    SCENE_CONFIG,
    SCENE_RUN,
    SCENE_DONE,
    SCENE_COUNT,
} SceneId;

typedef enum {
    EVENT_FILE_SELECTED,
    EVENT_SIGNAL_SELECTED,
    EVENT_MODE_CHANGED,
    EVENT_TIMER_CONFIGURED,
    EVENT_TIMER_TICK,
    EVENT_FIRE_SIGNAL,
    EVENT_DONE,
} EvId;

void scene_browse_enter(void*);
bool scene_browse_event(void*, SceneManagerEvent);
void scene_browse_exit(void*);
void scene_select_enter(void*);
bool scene_select_event(void*, SceneManagerEvent);
void scene_select_exit(void*);
void scene_cfg_enter(void*);
bool scene_cfg_event(void*, SceneManagerEvent);
void scene_cfg_exit(void*);
void scene_run_enter(void*);
bool scene_run_event(void*, SceneManagerEvent);
void scene_run_exit(void*);
void scene_done_enter(void*);
bool scene_done_event(void*, SceneManagerEvent);
void scene_done_exit(void*);
