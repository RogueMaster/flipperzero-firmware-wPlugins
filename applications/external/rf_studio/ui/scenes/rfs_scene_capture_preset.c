#include "../../rf_studio_i.h"

/* Modulation presets come from the firmware settings, including user-defined custom presets. */

void rfs_scene_capture_preset_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Modulation");
    size_t count = rfs_radio_preset_count(app->radio);
    uint32_t selected = 0;
    for(size_t i = 0; i < count; i++) {
        const char* name = rfs_radio_preset_name(app->radio, i);
        submenu_add_item(submenu, name, i, rfs_app_submenu_callback, app);
        if(strcmp(name, app->capture_preset) == 0) selected = i;
    }
    submenu_set_selected_item(submenu, selected);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_capture_preset_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    size_t index = event.event - RfsEventMenuBase;
    if(index < rfs_radio_preset_count(app->radio)) {
        strlcpy(
            app->capture_preset,
            rfs_radio_preset_name(app->radio, index),
            sizeof(app->capture_preset));
    }
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, RfsSceneCapture);
    return true;
}

void rfs_scene_capture_preset_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
