#include "../hermes_i.h"

typedef enum {
    StartIndexDetect,
    StartIndexManual,
    StartIndexSelfTest,
    StartIndexWiring,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void hermes_scene_start_callback(void* context, uint32_t index) {
    HermesApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hermes_scene_start_on_enter(void* context) {
    HermesApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Hermes");

    submenu_add_item(submenu, "Detect Baud", StartIndexDetect, hermes_scene_start_callback, app);
    submenu_add_item(
        submenu, "Manual Console", StartIndexManual, hermes_scene_start_callback, app);
    submenu_add_item(submenu, "Self Test", StartIndexSelfTest, hermes_scene_start_callback, app);
    submenu_add_item(submenu, "Wiring Guide", StartIndexWiring, hermes_scene_start_callback, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, hermes_scene_start_callback, app);
    submenu_add_item(submenu, "About", StartIndexAbout, hermes_scene_start_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, HermesSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewSubmenu);
}

bool hermes_scene_start_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, HermesSceneStart, event.event);

        switch(event.event) {
        case StartIndexDetect:
            scene_manager_next_scene(app->scene_manager, HermesSceneDetect);
            return true;
        case StartIndexManual:
            scene_manager_next_scene(app->scene_manager, HermesSceneManual);
            return true;
        case StartIndexSelfTest:
            scene_manager_next_scene(app->scene_manager, HermesSceneSelfTest);
            return true;
        case StartIndexWiring:
            scene_manager_next_scene(app->scene_manager, HermesSceneWiring);
            return true;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, HermesSceneSettings);
            return true;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, HermesSceneAbout);
            return true;
        default:
            break;
        }
    }

    return false;
}

void hermes_scene_start_on_exit(void* context) {
    HermesApp* app = context;
    submenu_reset(app->submenu);
}
