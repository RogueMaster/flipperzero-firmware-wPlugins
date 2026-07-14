#include "scenes.h"
#include "../application.h"

// Scene on_enter handlers array
void (*const tonuino_scene_on_enter_handlers[])(void*) = {
    tonuino_scene_main_menu_on_enter,
    tonuino_scene_set_folder_on_enter,
    tonuino_scene_set_mode_on_enter,
    tonuino_scene_set_special1_on_enter,
    tonuino_scene_set_special2_on_enter,
    tonuino_scene_view_card_on_enter,
    tonuino_scene_read_card_waiting_on_enter,
    tonuino_scene_read_card_result_on_enter,
    tonuino_scene_write_card_waiting_on_enter,
    tonuino_scene_write_card_result_on_enter,
    tonuino_scene_rapid_write_on_enter,
    tonuino_scene_about_on_enter,
};

// Scene on_event handlers array
bool (*const tonuino_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    tonuino_scene_main_menu_on_event,
    tonuino_scene_set_folder_on_event,
    tonuino_scene_set_mode_on_event,
    tonuino_scene_set_special1_on_event,
    tonuino_scene_set_special2_on_event,
    tonuino_scene_view_card_on_event,
    tonuino_scene_read_card_waiting_on_event,
    tonuino_scene_read_card_result_on_event,
    tonuino_scene_write_card_waiting_on_event,
    tonuino_scene_write_card_result_on_event,
    tonuino_scene_rapid_write_on_event,
    tonuino_scene_about_on_event,
};

// Scene on_exit handlers array
void (*const tonuino_scene_on_exit_handlers[])(void*) = {
    tonuino_scene_main_menu_on_exit,
    tonuino_scene_set_folder_on_exit,
    tonuino_scene_set_mode_on_exit,
    tonuino_scene_set_special1_on_exit,
    tonuino_scene_set_special2_on_exit,
    tonuino_scene_view_card_on_exit,
    tonuino_scene_read_card_waiting_on_exit,
    tonuino_scene_read_card_result_on_exit,
    tonuino_scene_write_card_waiting_on_exit,
    tonuino_scene_write_card_result_on_exit,
    tonuino_scene_rapid_write_on_exit,
    tonuino_scene_about_on_exit,
};

// Scene handlers structure
const SceneManagerHandlers tonuino_scene_handlers = {
    .on_enter_handlers = tonuino_scene_on_enter_handlers,
    .on_event_handlers = tonuino_scene_on_event_handlers,
    .on_exit_handlers = tonuino_scene_on_exit_handlers,
    .scene_num = TonuinoSceneCount,
};
