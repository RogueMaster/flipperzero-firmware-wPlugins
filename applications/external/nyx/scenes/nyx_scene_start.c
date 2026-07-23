#include "../nyx_i.h"

typedef enum {
    StartIndexSweep,
    StartIndexProbe,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void nyx_scene_start_submenu_cb(void* context, uint32_t index) {
    NyxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nyx_scene_start_on_enter(void* context) {
    NyxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Nyx");
    submenu_add_item(submenu, "Sweep", StartIndexSweep, nyx_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Probe Setup", StartIndexProbe, nyx_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, nyx_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, nyx_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, NyxSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, NyxViewSubmenu);
}

bool nyx_scene_start_on_event(void* context, SceneManagerEvent event) {
    NyxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, NyxSceneStart, event.event);
        switch(event.event) {
        case StartIndexSweep:
            scene_manager_next_scene(app->scene_manager, NyxSceneSweep);
            consumed = true;
            break;
        case StartIndexProbe:
            scene_manager_next_scene(app->scene_manager, NyxSceneProbe);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, NyxSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, NyxSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void nyx_scene_start_on_exit(void* context) {
    NyxApp* app = context;
    submenu_reset(app->submenu);
}
