#include "../bt_audio.h"

static void bt_audio_scene_device_list_submenu_callback(void* context, uint32_t index) {
    BtAudio* app = context;
    app->selected_device = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventConnect);
}

void bt_audio_scene_device_list_on_enter(void* context) {
    BtAudio* app = context;
    Submenu* submenu = app->submenu;

    // Add devices to submenu
    for(uint8_t i = 0; i < app->device_count; i++) {
        submenu_add_item(
            submenu, app->device_list[i], i, bt_audio_scene_device_list_submenu_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
}

bool bt_audio_scene_device_list_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtAudioEventConnect) {
            scene_manager_next_scene(app->scene_manager, BtAudioSceneConnect);
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_device_list_on_exit(void* context) {
    BtAudio* app = context;
    submenu_reset(app->submenu);
}
