#include "../gauntlet_i.h"

typedef enum {
    StartIndexPlay,
    StartIndexToolkit,
    StartIndexProgress,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void gauntlet_scene_start_submenu_cb(void* context, uint32_t index) {
    GauntletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void gauntlet_scene_start_on_enter(void* context) {
    GauntletApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Gauntlet");
    submenu_add_item(submenu, "Play", StartIndexPlay, gauntlet_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Toolkit", StartIndexToolkit, gauntlet_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Progress", StartIndexProgress, gauntlet_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Settings", StartIndexSettings, gauntlet_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "How it works", StartIndexAbout, gauntlet_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, GauntletSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewSubmenu);
}

bool gauntlet_scene_start_on_event(void* context, SceneManagerEvent event) {
    GauntletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, GauntletSceneStart, event.event);
        switch(event.event) {
        case StartIndexPlay:
            scene_manager_next_scene(app->scene_manager, GauntletScenePacks);
            consumed = true;
            break;
        case StartIndexToolkit:
            app->tool_from_challenge = false;
            scene_manager_next_scene(app->scene_manager, GauntletSceneToolInput);
            consumed = true;
            break;
        case StartIndexProgress:
            scene_manager_next_scene(app->scene_manager, GauntletSceneProgress);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, GauntletSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, GauntletSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void gauntlet_scene_start_on_exit(void* context) {
    GauntletApp* app = context;
    submenu_reset(app->submenu);
}
