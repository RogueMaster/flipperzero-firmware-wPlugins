#include "../faraday_i.h"

typedef enum {
    StartIndexSubGhz,
    StartIndexNfc,
    StartIndexHunt,
    StartIndexResults,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void faraday_scene_start_submenu_cb(void* context, uint32_t index) {
    FaradayApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void faraday_scene_start_on_enter(void* context) {
    FaradayApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Faraday");
    submenu_add_item(
        submenu, "Test Sub-GHz (key fob)", StartIndexSubGhz, faraday_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Test NFC (card)", StartIndexNfc, faraday_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Leak hunt (Sub-GHz)", StartIndexHunt, faraday_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Saved results", StartIndexResults, faraday_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, faraday_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, faraday_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, FaradaySceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewSubmenu);
}

bool faraday_scene_start_on_event(void* context, SceneManagerEvent event) {
    FaradayApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, FaradaySceneStart, event.event);
        switch(event.event) {
        case StartIndexSubGhz:
            scene_manager_next_scene(app->scene_manager, FaradaySceneSubGhz);
            consumed = true;
            break;
        case StartIndexNfc:
            scene_manager_next_scene(app->scene_manager, FaradaySceneNfc);
            consumed = true;
            break;
        case StartIndexHunt:
            scene_manager_next_scene(app->scene_manager, FaradaySceneHunt);
            consumed = true;
            break;
        case StartIndexResults:
            scene_manager_next_scene(app->scene_manager, FaradaySceneResults);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, FaradaySceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, FaradaySceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void faraday_scene_start_on_exit(void* context) {
    FaradayApp* app = context;
    submenu_reset(app->submenu);
}
