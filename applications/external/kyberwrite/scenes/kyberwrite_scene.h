#pragma once

#include <gui/scene_manager.h>
#include "../kyberwrite_app.h"

typedef enum {
    KyberEventMenuFullInit,
    KyberEventMenuQuickChange,
    KyberEventMenuRead,
    KyberEventColorSelected,
    KyberEventWriteStepDone,
    KyberEventWriteComplete,
    KyberEventWriteFailed,
    KyberEventResultBack,
    KyberEventReadDone,
    KyberEventReadBack,
    KyberEventSplashNext,
    KyberEventHintNext,
    KyberEventShowHint,
    KyberEventAppExit,
} KyberCustomEvent;


void kyberwrite_scene_splash_on_enter(void* context);
bool kyberwrite_scene_splash_on_event(void* context, SceneManagerEvent event);
void kyberwrite_scene_splash_on_exit(void* context);

void kyberwrite_scene_hint_on_enter(void* context);
bool kyberwrite_scene_hint_on_event(void* context, SceneManagerEvent event);
void kyberwrite_scene_hint_on_exit(void* context);

void kyberwrite_scene_menu_on_enter(void* context);
bool kyberwrite_scene_menu_on_event(void* context, SceneManagerEvent event);
void kyberwrite_scene_menu_on_exit(void* context);

void kyberwrite_scene_color_on_enter(void* context);
bool kyberwrite_scene_color_on_event(void* context, SceneManagerEvent event);
void kyberwrite_scene_color_on_exit(void* context);

void kyberwrite_scene_writing_on_enter(void* context);
bool kyberwrite_scene_writing_on_event(void* context, SceneManagerEvent event);
void kyberwrite_scene_writing_on_exit(void* context);

void kyberwrite_scene_result_on_enter(void* context);
bool kyberwrite_scene_result_on_event(void* context, SceneManagerEvent event);
void kyberwrite_scene_result_on_exit(void* context);

void kyberwrite_scene_read_on_enter(void* context);
bool kyberwrite_scene_read_on_event(void* context, SceneManagerEvent event);
void kyberwrite_scene_read_on_exit(void* context);

extern const SceneManagerHandlers kyberwrite_scene_handlers;
