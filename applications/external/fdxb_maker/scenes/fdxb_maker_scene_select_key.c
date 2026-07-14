#include "../fdxb_maker.h"

void fdxb_maker_scene_select_key_on_enter(void* context) {
    FdxbMaker* app = context;

    scene_manager_set_scene_state(app->scene_manager, FdxbMakerSceneStart, FdxbMakerMenuIndexEdit);

    furi_string_reset(app->file_name);

    if(fdxb_maker_load_key_from_file_select(app)) {
        scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveCountry);
    } else {
        scene_manager_set_scene_state(
            app->scene_manager, FdxbMakerSceneStart, FdxbMakerMenuIndexEdit);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FdxbMakerSceneStart);
    }
}

bool fdxb_maker_scene_select_key_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

void fdxb_maker_scene_select_key_on_exit(void* context) {
    UNUSED(context);
}
