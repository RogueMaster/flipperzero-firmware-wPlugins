#include "../cerberus_i.h"

typedef enum {
    StartItemMonitor,
    StartItemStats,
    StartItemAlerts,
    StartItemSettings,
    StartItemAbout,
} StartItem;

static void cerberus_scene_start_submenu_callback(void* context, uint32_t index) {
    Cerberus* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cerberus_scene_start_on_enter(void* context) {
    Cerberus* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "C E R B E R U S");
    submenu_add_item(
        submenu, "Watch", StartItemMonitor, cerberus_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Stats", StartItemStats, cerberus_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Alert Log", StartItemAlerts, cerberus_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Settings", StartItemSettings, cerberus_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "About", StartItemAbout, cerberus_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, CerberusSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, CerberusViewSubmenu);
}

bool cerberus_scene_start_on_event(void* context, SceneManagerEvent event) {
    Cerberus* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CerberusSceneStart, event.event);
        switch(event.event) {
        case StartItemMonitor:
            scene_manager_next_scene(app->scene_manager, CerberusSceneMonitor);
            break;
        case StartItemStats:
            scene_manager_next_scene(app->scene_manager, CerberusSceneStats);
            break;
        case StartItemAlerts:
            scene_manager_next_scene(app->scene_manager, CerberusSceneAlerts);
            break;
        case StartItemSettings:
            scene_manager_next_scene(app->scene_manager, CerberusSceneSettings);
            break;
        case StartItemAbout:
            scene_manager_next_scene(app->scene_manager, CerberusSceneAbout);
            break;
        default:
            break;
        }
        consumed = true;
    }

    return consumed;
}

void cerberus_scene_start_on_exit(void* context) {
    Cerberus* app = context;
    submenu_reset(app->submenu);
}
