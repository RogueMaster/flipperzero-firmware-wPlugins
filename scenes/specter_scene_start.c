#include "../specter_i.h"

typedef enum {
    StartIndexSweep,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void specter_scene_start_submenu_cb(void* context, uint32_t index) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void specter_scene_start_on_enter(void* context) {
    SpecterApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Specter");
    submenu_add_item(submenu, "Sweep", StartIndexSweep, specter_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Settings", StartIndexSettings, specter_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, specter_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, SpecterSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewSubmenu);
}

bool specter_scene_start_on_event(void* context, SceneManagerEvent event) {
    SpecterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, SpecterSceneStart, event.event);
        switch(event.event) {
        case StartIndexSweep:
            scene_manager_next_scene(app->scene_manager, SpecterSceneSweep);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, SpecterSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, SpecterSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void specter_scene_start_on_exit(void* context) {
    SpecterApp* app = context;
    submenu_reset(app->submenu);
}
