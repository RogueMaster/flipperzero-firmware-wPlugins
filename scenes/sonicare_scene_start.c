#include "../uk_mbirth_sonicare.h"
#include <dolphin/dolphin.h>

static void sonicare_scene_start_submenu_callback(void* context, uint32_t index) {
    Sonicare* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sonicare_scene_start_on_enter(void* context) {
    Sonicare* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu,
        "Extra Actions",
        SonicareMenuIndexExtraActions,
        sonicare_scene_start_submenu_callback,
        app);
    submenu_add_item(submenu, "Read Brush Head", SonicareMenuIndexRead, sonicare_scene_start_submenu_callback, app);
    submenu_add_item(submenu, "Write Brush Head", SonicareMenuIndexSaved, sonicare_scene_start_submenu_callback, app);
    submenu_add_item(submenu, "Add Manually", SonicareMenuIndexAddManually, sonicare_scene_start_submenu_callback, app);


    // clear key
    //furi_string_reset(app->file_name);
    //app->protocol_id = PROTOCOL_NO;
    //app->read_type = SONICAREWorkerReadTypeAuto;
    submenu_set_selected_item(submenu, scene_manager_get_scene_state(app->scene_manager, SonicareSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewSubmenu);
}

bool sonicare_scene_start_on_event(void* context, SceneManagerEvent event) {
    Sonicare* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == SonicareMenuIndexRead) {
            scene_manager_set_scene_state(app->scene_manager, SonicareSceneStart, SonicareMenuIndexRead);
            scene_manager_next_scene(app->scene_manager, SonicareSceneRead);
            dolphin_deed(DolphinDeedNfcRead);
            consumed = true;
        }
    }

    return consumed;
}

void sonicare_scene_start_on_exit(void* context) {
    Sonicare* app = context;

    submenu_reset(app->submenu);
}
