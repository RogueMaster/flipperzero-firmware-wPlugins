#include "../glitch_trigger_i.h"

typedef enum {
    StartIndexTrigger,
    StartIndexConfigure,
    StartIndexSweep,
    StartIndexWiring,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void glitch_scene_start_submenu_cb(void* context, uint32_t index) {
    GlitchApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void glitch_scene_start_on_enter(void* context) {
    GlitchApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Glitch Trigger");
    submenu_add_item(submenu, "Trigger", StartIndexTrigger, glitch_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Configure", StartIndexConfigure, glitch_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Sweep", StartIndexSweep, glitch_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Wiring", StartIndexWiring, glitch_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, glitch_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, glitch_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, GlitchSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewSubmenu);
}

bool glitch_scene_start_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, GlitchSceneStart, event.event);
        switch(event.event) {
        case StartIndexTrigger:
            scene_manager_next_scene(app->scene_manager, GlitchSceneTrigger);
            consumed = true;
            break;
        case StartIndexConfigure:
            scene_manager_next_scene(app->scene_manager, GlitchSceneParams);
            consumed = true;
            break;
        case StartIndexSweep:
            scene_manager_next_scene(app->scene_manager, GlitchSceneSweep);
            consumed = true;
            break;
        case StartIndexWiring:
            scene_manager_next_scene(app->scene_manager, GlitchSceneWiring);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, GlitchSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, GlitchSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void glitch_scene_start_on_exit(void* context) {
    GlitchApp* app = context;
    submenu_reset(app->submenu);
}
