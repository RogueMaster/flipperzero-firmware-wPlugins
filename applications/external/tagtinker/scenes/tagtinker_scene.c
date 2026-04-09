/*
 * Scene handler table
 */

#include "tagtinker_scene.h"

void(*const tagtinker_scene_on_enter_handlers[])(void*) = {
    tagtinker_scene_warning_on_enter,
    tagtinker_scene_main_menu_on_enter,
    tagtinker_scene_settings_on_enter,
    tagtinker_scene_broadcast_menu_on_enter,
    tagtinker_scene_broadcast_on_enter,
    tagtinker_scene_target_menu_on_enter,
    tagtinker_scene_target_actions_on_enter,
    tagtinker_scene_barcode_input_on_enter,
    tagtinker_scene_text_input_on_enter,
    tagtinker_scene_preset_list_on_enter,
    tagtinker_scene_synced_image_list_on_enter,
    tagtinker_scene_size_picker_on_enter,
    tagtinker_scene_image_options_on_enter,
    tagtinker_scene_transmit_on_enter,
    tagtinker_scene_about_on_enter,
};

bool(*const tagtinker_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    tagtinker_scene_warning_on_event,
    tagtinker_scene_main_menu_on_event,
    tagtinker_scene_settings_on_event,
    tagtinker_scene_broadcast_menu_on_event,
    tagtinker_scene_broadcast_on_event,
    tagtinker_scene_target_menu_on_event,
    tagtinker_scene_target_actions_on_event,
    tagtinker_scene_barcode_input_on_event,
    tagtinker_scene_text_input_on_event,
    tagtinker_scene_preset_list_on_event,
    tagtinker_scene_synced_image_list_on_event,
    tagtinker_scene_size_picker_on_event,
    tagtinker_scene_image_options_on_event,
    tagtinker_scene_transmit_on_event,
    tagtinker_scene_about_on_event,
};

void(*const tagtinker_scene_on_exit_handlers[])(void*) = {
    tagtinker_scene_warning_on_exit,
    tagtinker_scene_main_menu_on_exit,
    tagtinker_scene_settings_on_exit,
    tagtinker_scene_broadcast_menu_on_exit,
    tagtinker_scene_broadcast_on_exit,
    tagtinker_scene_target_menu_on_exit,
    tagtinker_scene_target_actions_on_exit,
    tagtinker_scene_barcode_input_on_exit,
    tagtinker_scene_text_input_on_exit,
    tagtinker_scene_preset_list_on_exit,
    tagtinker_scene_synced_image_list_on_exit,
    tagtinker_scene_size_picker_on_exit,
    tagtinker_scene_image_options_on_exit,
    tagtinker_scene_transmit_on_exit,
    tagtinker_scene_about_on_exit,
};

const SceneManagerHandlers tagtinker_scene_handlers = {
    .on_enter_handlers = tagtinker_scene_on_enter_handlers,
    .on_event_handlers = tagtinker_scene_on_event_handlers,
    .on_exit_handlers = tagtinker_scene_on_exit_handlers,
    .scene_num = TagTinkerSceneCount,
};
