// scenes/protopirate_scene_emulate.c
#include "../protopirate_app_i.h"

#ifdef ENABLE_EMULATE_FEATURE

#include "../helpers/protopirate_storage.h"
#include "../protopirate_history.h"

#define TAG "PPSceneEmulate"

void protopirate_scene_emulate_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!shared_plugin_load(
           &app->running_plugin_flipper_application,
           &app->running_plugin,
           ProtoPirateSharedPluginsEmulate,
           NULL)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    } else {
        app->running_plugin.shared_plugin->set_host_api(&protopirate_shared_plugin_host_api);
        app->running_plugin.shared_plugin->on_enter(app);
    }
}

bool protopirate_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;

    bool consumed = false;
    //Handle Saved event in plugin.
    if(app->running_plugin.shared_plugin &&
       app->running_plugin.shared_plugin->on_event(app, event)) {
        consumed = true;
    } else {
        consumed = shared_plugin_handle_navigation_events(
            app->scene_manager, app->view_dispatcher, event);
    }
    return consumed;
}

void protopirate_scene_emulate_on_exit(void* context) {
    ProtoPirateApp* app = context;

    /*if(app->running_plugin.shared_plugin && app->running_plugin.shared_plugin->release) {
        app->running_plugin.shared_plugin->release(app);
    }*/ //free calls it anyway
    if(app->running_plugin.shared_plugin && app->running_plugin.shared_plugin->on_exit) {
        app->running_plugin.shared_plugin->on_exit(app);
    }
    shared_plugin_unload(&app->running_plugin_flipper_application, &app->running_plugin);
}

#endif // ENABLE_EMULATE_FEATURE
