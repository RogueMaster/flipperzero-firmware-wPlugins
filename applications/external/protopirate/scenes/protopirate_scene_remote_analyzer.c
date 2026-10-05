#include "../protopirate_app_i.h"
#ifdef ENABLE_REMOTE_ANALYZER
#include <lib/subghz/subghz_setting.h>

#define TAG "PPRemoteAnalyzer"

void protopirate_scene_remote_analyzer_on_enter(void* context) {
    ProtoPirateApp* app = context;

    FURI_LOG_D("TAG", "Loading Plugin");
    if(!shared_plugin_load(
           &app->running_plugin_flipper_application,
           &app->running_plugin,
           ProtoPirateSharedPluginsRemoteAnalyzer,
           NULL)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    app->running_plugin.shared_plugin->set_host_api(&protopirate_shared_plugin_host_api);

    FURI_LOG_D("TAG", "Calling Plugin Entry Point");
    app->running_plugin.shared_plugin->on_enter(app);
}

bool protopirate_scene_remote_analyzer_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;

    //Handle event in plugin.
    if(app->running_plugin.shared_plugin->on_event(app, event)) {
        return true;
    } else {
        return shared_plugin_handle_navigation_events(
            app->scene_manager, app->view_dispatcher, event);
    }
}

void protopirate_scene_remote_analyzer_on_exit(void* context) {
    ProtoPirateApp* app = context;

    app->running_plugin.shared_plugin->on_exit(app);

    shared_plugin_unload(&app->running_plugin_flipper_application, &app->running_plugin);
}
#endif
