#include "scenes.h"

void (*const loteria_on_enter_handlers[])(void*) = {
    loteria_scene_welcome_on_enter,
    loteria_scene_main_menu_on_enter,
    loteria_scene_game_on_enter,
    loteria_scene_history_on_enter,
    loteria_scene_stats_on_enter,
    loteria_scene_our_story_on_enter,
    loteria_scene_settings_on_enter,
    loteria_scene_about_on_enter,
    loteria_scene_all_cards_on_enter,
    loteria_scene_game_end_on_enter,
};

bool (*const loteria_on_event_handlers[])(void*, SceneManagerEvent) = {
    loteria_scene_welcome_on_event,
    loteria_scene_main_menu_on_event,
    loteria_scene_game_on_event,
    loteria_scene_history_on_event,
    loteria_scene_stats_on_event,
    loteria_scene_our_story_on_event,
    loteria_scene_settings_on_event,
    loteria_scene_about_on_event,
    loteria_scene_all_cards_on_event,
    loteria_scene_game_end_on_event,
};

void (*const loteria_on_exit_handlers[])(void*) = {
    loteria_scene_welcome_on_exit,
    loteria_scene_main_menu_on_exit,
    loteria_scene_game_on_exit,
    loteria_scene_history_on_exit,
    loteria_scene_stats_on_exit,
    loteria_scene_our_story_on_exit,
    loteria_scene_settings_on_exit,
    loteria_scene_about_on_exit,
    loteria_scene_all_cards_on_exit,
    loteria_scene_game_end_on_exit,
};

const SceneManagerHandlers loteria_scene_handlers = {
    .on_enter_handlers = loteria_on_enter_handlers,
    .on_event_handlers = loteria_on_event_handlers,
    .on_exit_handlers = loteria_on_exit_handlers,
    .scene_num = LoteriaSceneCount,
};
