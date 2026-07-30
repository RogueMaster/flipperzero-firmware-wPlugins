#include "wol_flipper.h"

typedef enum {
    StartIndexWake,
    StartIndexTargets,
    StartIndexWifi,
    StartIndexBoard,
    StartIndexAbout,
} StartIndex;

static void wol_scene_start_callback(void* context, uint32_t index) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void wol_scene_start_on_enter(void* context) {
    WolApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "WoL Flipper");
    submenu_add_item(app->submenu, "Wake device", StartIndexWake, wol_scene_start_callback, app);
    submenu_add_item(app->submenu, "Targets", StartIndexTargets, wol_scene_start_callback, app);
    submenu_add_item(app->submenu, "Wi-Fi setup", StartIndexWifi, wol_scene_start_callback, app);
    submenu_add_item(app->submenu, "ESP board", StartIndexBoard, wol_scene_start_callback, app);
    submenu_add_item(app->submenu, "About", StartIndexAbout, wol_scene_start_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, WolSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewSubmenu);
}

bool wol_scene_start_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    // late events from the send worker must not poison the menu selection
    if(event.event > StartIndexAbout) return false;

    scene_manager_set_scene_state(app->scene_manager, WolSceneStart, event.event);

    switch(event.event) {
    case StartIndexWake:
        app->list_mode_wake = true;
        scene_manager_next_scene(app->scene_manager, WolSceneTargets);
        return true;
    case StartIndexTargets:
        app->list_mode_wake = false;
        scene_manager_next_scene(app->scene_manager, WolSceneTargets);
        return true;
    case StartIndexWifi:
        scene_manager_next_scene(app->scene_manager, WolSceneWifi);
        return true;
    case StartIndexBoard:
        scene_manager_next_scene(app->scene_manager, WolSceneBoard);
        return true;
    case StartIndexAbout:
        scene_manager_next_scene(app->scene_manager, WolSceneAbout);
        return true;
    default:
        return false;
    }
}

void wol_scene_start_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}
