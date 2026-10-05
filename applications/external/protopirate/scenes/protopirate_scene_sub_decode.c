// scenes/protopirate_scene_sub_decode.c
#include "../protopirate_app_i.h"
#ifdef ENABLE_SUB_DECODE_SCENE

void protopirate_scene_sub_decode_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(!shared_plugin_load(
           (void**)&app->plugin_flipper_application,
           &app->shared_plugin,
           ProtoPirateSharedPluginsSubDecode,
           NULL)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    ((ProtoPirateSharedPlugin*)app->shared_plugin)
        ->set_host_api(&protopirate_shared_plugin_host_api);
    ((ProtoPirateSharedPlugin*)app->shared_plugin)->on_enter(app);
}

bool protopirate_scene_sub_decode_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    if(!app || !app->shared_plugin || !((ProtoPirateSharedPlugin*)app->shared_plugin)->on_event) {
        return false;
    }

    if(((ProtoPirateSharedPlugin*)app->shared_plugin)->on_event(app, event)) {
        return true;
    } else {
        return shared_plugin_handle_navigation_events(
            app->scene_manager, app->view_dispatcher, event);
    }
}

void protopirate_scene_sub_decode_on_exit(void* context) {
    ProtoPirateApp* app = context;
    if(!app) return;

    if(app->shared_plugin) {
        if(((ProtoPirateSharedPlugin*)app->shared_plugin)->on_exit) {
            ((ProtoPirateSharedPlugin*)app->shared_plugin)->on_exit(app);
        }
        if(((ProtoPirateSharedPlugin*)app->shared_plugin)->release) {
            ((ProtoPirateSharedPlugin*)app->shared_plugin)->release(app);
        }
    }

    shared_plugin_unload((void**)&app->plugin_flipper_application, &app->shared_plugin);
}
#endif
