// scenes/protopirate_scene_receiver_config.c
#include "protopirate_app_i.h"
#include "plugins/protopirate_config_plugin.h"
#include "../helpers/protopirate_protocol_plugin_host.h"

static const ProtoPirateConfigSceneHostApi protopirate_config_scene_host_api = {
    .protopirate_refresh_protocol_registry = protopirate_refresh_protocol_registry,
    .protopirate_preset_init = protopirate_preset_init,
};

void protopirate_scene_receiver_config_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!shared_plugin_load(
           (void**)&app->plugin_flipper_application,
           (const void**)&app->config_plugin,
           ProtoPirateSharedPluginsConfig,
           NULL)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    app->config_plugin->set_host_api(&protopirate_config_scene_host_api);
    //We have to grab this from the scene manager, from inside the plugin it just doesnt work!
    bool show_lock_keyboard =
        (scene_manager_get_scene_state(app->scene_manager, ProtoPirateSceneReceiverConfig) == 1);
    app->config_plugin->on_enter(app, show_lock_keyboard);
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
    shared_plugin_unload(
        (void**)&app->plugin_flipper_application, (const void**)&app->config_plugin);
}
