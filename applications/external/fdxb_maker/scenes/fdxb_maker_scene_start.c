#include "../fdxb_maker.h"

static void fdxb_maker_scene_start_submenu_callback(void* context, uint32_t index) {
    FdxbMaker* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void fdxb_maker_scene_start_on_enter(void* context) {
    FdxbMaker* app = context;

    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu, "New", FdxbMakerMenuIndexNew, fdxb_maker_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Edit", FdxbMakerMenuIndexEdit, fdxb_maker_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, FdxbMakerSceneStart));

    furi_string_reset(app->file_name);

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewSubmenu);
}

bool fdxb_maker_scene_start_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FdxbMakerMenuIndexNew) {
            scene_manager_set_scene_state(
                app->scene_manager, FdxbMakerSceneStart, FdxbMakerMenuIndexNew);
            scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveCountry);
            consumed = true;
        } else if(event.event == FdxbMakerMenuIndexEdit) {
            scene_manager_set_scene_state(
                app->scene_manager, FdxbMakerSceneStart, FdxbMakerMenuIndexEdit);
            furi_string_set(app->file_path, LFRFID_APP_FOLDER);
            scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSelectKey);
            consumed = true;
        }
    }

    return consumed;
}

void fdxb_maker_scene_start_on_exit(void* context) {
    FdxbMaker* app = context;

    submenu_reset(app->submenu);
}
