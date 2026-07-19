#include "../ghosttag_i.h"

typedef enum {
    StartIndexHunt,
    StartIndexDetections,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void ghosttag_scene_start_submenu_cb(void* context, uint32_t index) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ghosttag_scene_start_on_enter(void* context) {
    GhostTagApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "GhostTag");
    submenu_add_item(submenu, "Hunt (Scan)", StartIndexHunt, ghosttag_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Detections", StartIndexDetections, ghosttag_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, ghosttag_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, ghosttag_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, GhostTagSceneStart));

    // Returning to the menu ends the hunt session.
    if(uart_link_is_running(app->uart)) {
        uart_link_send_command(app->uart, "STOP\n");
        uart_link_stop(app->uart);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewSubmenu);
}

bool ghosttag_scene_start_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, GhostTagSceneStart, event.event);
        switch(event.event) {
        case StartIndexHunt:
            scene_manager_next_scene(app->scene_manager, GhostTagSceneScan);
            consumed = true;
            break;
        case StartIndexDetections:
            scene_manager_next_scene(app->scene_manager, GhostTagSceneList);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, GhostTagSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, GhostTagSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void ghosttag_scene_start_on_exit(void* context) {
    GhostTagApp* app = context;
    submenu_reset(app->submenu);
}
