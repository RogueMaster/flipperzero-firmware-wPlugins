// scenes/protopirate_scene_welcome.c
// In-app documentation: protocoles par marque et modele de voiture
#include "../protopirate_app_i.h"
#ifdef ENABLE_WELCOME_SCREEN
#include <notification/notification_messages.h>

#define TAG "PPSceneWelcome"

void protopirate_scene_welcome_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    if(!protopirate_ensure_widget(app)) {
        FURI_LOG_E(TAG, "Failed to allocate widget");
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(!shared_plugin_load(
           (void**)&app->plugin_flipper_application,
           &app->shared_plugin,
           ProtoPirateSharedPluginsWelcome,
           NULL)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    ((ProtoPirateSharedPlugin*)app->shared_plugin)->on_enter(app);
}

bool protopirate_scene_welcome_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    //I can't set the next scene from inside the plugin, or it causes crazy crashes.
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    //Handle About event in plugin.
    return ((ProtoPirateSharedPlugin*)app->shared_plugin)->on_event(app, event);
}

void protopirate_scene_welcome_on_exit(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    shared_plugin_unload(
        (void**)&app->plugin_flipper_application, (const void**)&app->shared_plugin);

    widget_reset(app->widget);
}
#endif
