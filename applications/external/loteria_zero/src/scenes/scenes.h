#pragma once

#include "../loteria_zero.h"

extern const SceneManagerHandlers loteria_scene_handlers;

void loteria_scene_welcome_on_enter(void* context);
bool loteria_scene_welcome_on_event(void* context, SceneManagerEvent event);
void loteria_scene_welcome_on_exit(void* context);

void loteria_scene_main_menu_on_enter(void* context);
bool loteria_scene_main_menu_on_event(void* context, SceneManagerEvent event);
void loteria_scene_main_menu_on_exit(void* context);

void loteria_scene_game_on_enter(void* context);
bool loteria_scene_game_on_event(void* context, SceneManagerEvent event);
void loteria_scene_game_on_exit(void* context);

void loteria_scene_history_on_enter(void* context);
bool loteria_scene_history_on_event(void* context, SceneManagerEvent event);
void loteria_scene_history_on_exit(void* context);

void loteria_scene_stats_on_enter(void* context);
bool loteria_scene_stats_on_event(void* context, SceneManagerEvent event);
void loteria_scene_stats_on_exit(void* context);

void loteria_scene_our_story_on_enter(void* context);
bool loteria_scene_our_story_on_event(void* context, SceneManagerEvent event);
void loteria_scene_our_story_on_exit(void* context);

void loteria_scene_settings_on_enter(void* context);
bool loteria_scene_settings_on_event(void* context, SceneManagerEvent event);
void loteria_scene_settings_on_exit(void* context);

void loteria_scene_about_on_enter(void* context);
bool loteria_scene_about_on_event(void* context, SceneManagerEvent event);
void loteria_scene_about_on_exit(void* context);

void loteria_scene_all_cards_on_enter(void* context);
bool loteria_scene_all_cards_on_event(void* context, SceneManagerEvent event);
void loteria_scene_all_cards_on_exit(void* context);

void loteria_scene_game_end_on_enter(void* context);
bool loteria_scene_game_end_on_event(void* context, SceneManagerEvent event);
void loteria_scene_game_end_on_exit(void* context);
