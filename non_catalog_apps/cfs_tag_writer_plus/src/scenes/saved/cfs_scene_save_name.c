#include "src/core/cfs_app_i.h"

static void cfs_save_name_callback(void* context) {
    CfsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonSave);
}

void cfs_scene_save_name_on_enter(void* context) {
    CfsApp* app = context;
    if(app->save_name[0] == '\0') strncpy(app->save_name, "tag", sizeof(app->save_name));
    text_input_set_header_text(
        app->text_input, app->save_as_template ? "Name this template" : "Name this tag");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        cfs_save_name_callback,
        app,
        app->save_name,
        sizeof(app->save_name),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewTextInput);
}

bool cfs_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == CfsCustomEventButtonSave) {
        if(cfs_storage_exists_in(app->storage, cfs_app_save_dir(app), app->save_name)) {
            // Don't clobber silently: confirm the overwrite first.
            scene_manager_next_scene(app->scene_manager, CfsSceneSaveOverwrite);
        } else {
            cfs_app_save_pending(app);
            // Back past the save-kind choice, to the screen Save was pressed on.
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, cfs_app_save_invoker(app));
        }
        return true;
    }
    return false;
}

void cfs_scene_save_name_on_exit(void* context) {
    UNUSED(context);
}
