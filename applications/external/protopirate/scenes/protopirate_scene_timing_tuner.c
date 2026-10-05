// scenes/protopirate_scene_timing_tuner.c
#include "../protopirate_app_i.h"
#ifdef ENABLE_TIMING_TUNER_SCENE

void protopirate_scene_timing_tuner_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!shared_plugin_load(
           &app->running_plugin_flipper_application,
           &app->running_plugin,
           ProtoPirateSharedPluginsTimingTuner,
           NULL)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->running_plugin.shared_plugin->set_host_api(&protopirate_shared_plugin_host_api);
    app->running_plugin.shared_plugin->on_enter(app);
}

bool protopirate_scene_timing_tuner_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    if(!app || !app->running_plugin.shared_plugin ||
       !app->running_plugin.shared_plugin->on_event) {
        return false;
    }

    if(app->running_plugin.shared_plugin->on_event(app, event)) {
        return true;
    } else {
        return shared_plugin_handle_navigation_events(
            app->scene_manager, app->view_dispatcher, event);
    }
}

void protopirate_scene_timing_tuner_on_exit(void* context) {
    if(!context) return;
    ProtoPirateApp* app = context;

    if(app->running_plugin.shared_plugin) {
        if(app->running_plugin.shared_plugin->on_exit) {
            app->running_plugin.shared_plugin->on_exit(app);
        }
        if(app->running_plugin.shared_plugin->release) {
            app->running_plugin.shared_plugin->release(app);
        }
    }

    shared_plugin_unload(&app->running_plugin_flipper_application, &app->running_plugin);
}
#endif
