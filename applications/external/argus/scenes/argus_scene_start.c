#include "../argus_i.h"

typedef enum {
    StartIndexWatch,
    StartIndexTwins,
    StartIndexLog,
    StartIndexGuard,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void argus_scene_start_submenu_cb(void* context, uint32_t index) {
    ArgusApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void argus_scene_start_on_enter(void* context) {
    ArgusApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Argus");
    submenu_add_item(submenu, "Watch", StartIndexWatch, argus_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Evil Twin Scan", StartIndexTwins, argus_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Threat Log", StartIndexLog, argus_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Guarded SSID", StartIndexGuard, argus_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, argus_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, argus_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, ArgusSceneStart));

    /* Back at the menu = disarmed. Drop the radio link to free the USART. */
    argus_link_disarm(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ArgusViewSubmenu);
}

bool argus_scene_start_on_event(void* context, SceneManagerEvent event) {
    ArgusApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, ArgusSceneStart, event.event);
        switch(event.event) {
        case StartIndexWatch:
            scene_manager_next_scene(app->scene_manager, ArgusSceneMonitor);
            consumed = true;
            break;
        case StartIndexTwins:
            scene_manager_next_scene(app->scene_manager, ArgusSceneTwins);
            consumed = true;
            break;
        case StartIndexLog:
            scene_manager_next_scene(app->scene_manager, ArgusSceneLog);
            consumed = true;
            break;
        case StartIndexGuard:
            scene_manager_next_scene(app->scene_manager, ArgusSceneGuard);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, ArgusSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, ArgusSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void argus_scene_start_on_exit(void* context) {
    ArgusApp* app = context;
    submenu_reset(app->submenu);
}
