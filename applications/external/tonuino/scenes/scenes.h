#pragma once

#include <gui/scene_manager.h>

typedef enum {
    TonuinoSceneMainMenu,
    TonuinoSceneSetFolder,
    TonuinoSceneSetMode,
    TonuinoSceneSetSpecial1,
    TonuinoSceneSetSpecial2,
    TonuinoSceneViewCard,
    TonuinoSceneReadCardWaiting,
    TonuinoSceneReadCardResult,
    TonuinoSceneWriteCardWaiting,
    TonuinoSceneWriteCardResult,
    TonuinoSceneRapidWrite,
    TonuinoSceneAbout,
    TonuinoSceneCount,
} TonuinoScene;

extern const SceneManagerHandlers tonuino_scene_handlers;

// Scene handler declarations
void tonuino_scene_main_menu_on_enter(void* context);
bool tonuino_scene_main_menu_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_main_menu_on_exit(void* context);

void tonuino_scene_set_folder_on_enter(void* context);
bool tonuino_scene_set_folder_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_set_folder_on_exit(void* context);

void tonuino_scene_set_mode_on_enter(void* context);
bool tonuino_scene_set_mode_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_set_mode_on_exit(void* context);

void tonuino_scene_set_special1_on_enter(void* context);
bool tonuino_scene_set_special1_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_set_special1_on_exit(void* context);

void tonuino_scene_set_special2_on_enter(void* context);
bool tonuino_scene_set_special2_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_set_special2_on_exit(void* context);

void tonuino_scene_view_card_on_enter(void* context);
bool tonuino_scene_view_card_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_view_card_on_exit(void* context);

void tonuino_scene_read_card_waiting_on_enter(void* context);
bool tonuino_scene_read_card_waiting_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_read_card_waiting_on_exit(void* context);

void tonuino_scene_read_card_result_on_enter(void* context);
bool tonuino_scene_read_card_result_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_read_card_result_on_exit(void* context);

void tonuino_scene_write_card_waiting_on_enter(void* context);
bool tonuino_scene_write_card_waiting_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_write_card_waiting_on_exit(void* context);

void tonuino_scene_write_card_result_on_enter(void* context);
bool tonuino_scene_write_card_result_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_write_card_result_on_exit(void* context);

void tonuino_scene_rapid_write_on_enter(void* context);
bool tonuino_scene_rapid_write_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_rapid_write_on_exit(void* context);

void tonuino_scene_about_on_enter(void* context);
bool tonuino_scene_about_on_event(void* context, SceneManagerEvent event);
void tonuino_scene_about_on_exit(void* context);
