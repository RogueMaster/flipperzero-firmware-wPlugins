#include "../ir_maker.h"

static void ir_maker_scene_remote_name_callback(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void ir_maker_scene_remote_name_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    /* Seed with the current remote name, or a friendly default for a fresh one. */
    if(furi_string_empty(app->draft->remote_name)) {
        strncpy(app->name_buf, "Remote", sizeof(app->name_buf) - 1);
    } else {
        strncpy(
            app->name_buf,
            furi_string_get_cstr(app->draft->remote_name),
            sizeof(app->name_buf) - 1);
    }
    app->name_buf[sizeof(app->name_buf) - 1] = '\0';

    text_input_set_header_text(app->text_input, "Name the remote");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        ir_maker_scene_remote_name_callback,
        app,
        app->name_buf,
        sizeof(app->name_buf),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewTextInput);
}

bool ir_maker_scene_remote_name_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        /* Reserve a free file path and mirror its (possibly de-duplicated) base
         * name back into the draft so the header and the file agree. */
        ir_maker_find_vacant_path(app, app->name_buf, app->file_path);
        path_extract_filename_no_ext(
            furi_string_get_cstr(app->file_path), app->draft->remote_name);
        scene_manager_next_scene(app->scene_manager, IrMakerSceneDefaults);
        consumed = true;
    }

    return consumed;
}

void ir_maker_scene_remote_name_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    text_input_reset(app->text_input);
}
