#include "../openprinttag_i.h"

typedef enum {
    SubmenuIndexRead,
} SubmenuIndex;

static void openprinttag_scene_start_submenu_callback(void* context, uint32_t index) {
    OpenPrintTag* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void openprinttag_scene_start_on_enter(void* context) {
    OpenPrintTag* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu,
        "Read OpenPrintTag",
        SubmenuIndexRead,
        openprinttag_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, OpenPrintTagSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewSubmenu);
}

bool openprinttag_scene_start_on_event(void* context, SceneManagerEvent event) {
    OpenPrintTag* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexRead) {
            scene_manager_set_scene_state(
                app->scene_manager, OpenPrintTagSceneStart, SubmenuIndexRead);
            scene_manager_next_scene(app->scene_manager, OpenPrintTagSceneRead);
            consumed = true;
        }
    }

    return consumed;
}

void openprinttag_scene_start_on_exit(void* context) {
    OpenPrintTag* app = context;
    submenu_reset(app->submenu);
}
