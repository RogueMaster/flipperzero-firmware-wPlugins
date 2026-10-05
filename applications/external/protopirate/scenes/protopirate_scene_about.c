// scenes/protopirate_scene_about.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_settings.h"

void protopirate_scene_about_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!shared_plugin_load(
           &app->running_plugin_flipper_application,
           &app->running_plugin,
           ProtoPirateSharedPluginsAbout,
           NULL)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->running_plugin.shared_plugin->set_host_api(&protopirate_shared_plugin_host_api);
    app->running_plugin.shared_plugin->on_enter(app);
}

bool protopirate_scene_about_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    //I can't set the next scene from inside the plugin, or it causes crazy crashes.
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    //Handle About event in plugin.
    return app->running_plugin.shared_plugin->on_event(app, event);
}

void protopirate_scene_about_on_exit(void* context) {
    ProtoPirateApp* app = context;
    view_set_draw_callback(app->view_about, NULL);
    view_set_input_callback(app->view_about, NULL);
    view_set_context(app->view_about, NULL);
    shared_plugin_unload(&app->running_plugin_flipper_application, &app->running_plugin);
}
