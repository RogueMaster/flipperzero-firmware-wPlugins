// scenes/protopirate_scene_receiver_config.c
#include "../protopirate_app_i.h"

void protopirate_scene_receiver_config_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!config_plugin_load(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->config_plugin->on_enter(app);
}

bool protopirate_scene_receiver_config_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventSceneSettingLock) {
            app->lock = ProtoPirateLockOn;
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }
    return consumed;
}

void protopirate_scene_receiver_config_on_exit(void* context) {
    ProtoPirateApp* app = context;

    config_plugin_unload(app);
}
