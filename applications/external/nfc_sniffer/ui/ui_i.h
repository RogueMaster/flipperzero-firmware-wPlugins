#pragma once

#include "ui.h"

/// ids for all scenes used by the app
typedef enum {
    ProtocolSelectDisplay,
    LogDisplay,
    ui_count
} UIScene;

/// ids for the type of view used by the app
typedef enum {
    View_ProtocolSelectDisplay,
    View_LogDisplay,
    view_count
} UIView;

extern char* PROTOCOL_NAMES[NfcTechNum];

ProtocolSelectScene* protocol_select_scene_alloc();
void protocol_select_scene_on_enter(void* context);
bool protocol_select_scene_on_event(void* context, SceneManagerEvent event);
void protocol_select_scene_on_exit(void* context);
void protocol_select_scene_free(ProtocolSelectScene* protocol_select_scene);

LogScene* log_scene_alloc();
void log_scene_on_enter(void* context);
bool log_scene_on_event(void* context, SceneManagerEvent event);
void log_scene_on_exit(void* context);
void log_scene_free(LogScene* log_scene);
