// scenes/protopirate_scene_saved_info.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_bruteforce_host.h"
#include "../helpers/protopirate_storage.h"

#define TAG "PPSceneSavedInfo"

void protopirate_scene_saved_info_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!shared_plugin_load(
           &app->running_plugin_flipper_application,
           &app->running_plugin,
           ProtoPirateSharedPluginsSavedInfo,
           NULL)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->running_plugin.shared_plugin->set_host_api(&protopirate_shared_plugin_host_api);
    app->running_plugin.shared_plugin->on_enter(app);
    FURI_LOG_D(TAG, "Saved Info Enter Done");
}

bool protopirate_scene_saved_info_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = ((ProtoPirateApp*)context);

    //Handle event in plugin.
    if(app->running_plugin.shared_plugin &&
       app->running_plugin.shared_plugin->on_event(app, event)) {
        return true;
    } else {
        return shared_plugin_handle_navigation_events(
            app->scene_manager, app->view_dispatcher, event);
    }
}

void protopirate_scene_saved_info_on_exit(void* context) {
    ProtoPirateApp* app = context;
    widget_reset(app->widget);
    shared_plugin_unload(&app->running_plugin_flipper_application, &app->running_plugin);
    FURI_LOG_D(TAG, "Saved Info Exit Done.");
}
