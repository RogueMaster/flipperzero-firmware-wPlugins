#include "../app_user.h"

uint32_t select_index = 0;

void submenu_callback(void* context, uint32_t index) {
    App* app = context;

    storage_dir_read_index(app->storage, PATHLOGS, app->file_active->path, index);
    app->file_active->file_index = index;
    app->file_active->frame_index = 0;

    Stream* stream = file_stream_alloc(app->storage);
    file_stream_open(
        stream, furi_string_get_cstr(app->file_active->path), FSAM_READ, FSOM_OPEN_EXISTING);

    app->file_active->frames_count = stream_get_lines_count(stream);

    file_stream_close(stream);
    stream_free(stream);

    scene_manager_handle_custom_event(app->scene_manager, index);
}

void app_scene_read_logs_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "LOGS");

    FuriString* file_path = furi_string_alloc();
    FuriString* file_name = furi_string_alloc();

    uint64_t logs_count = storage_dir_get_files_count(app->storage, PATHLOGS);
    for(uint64_t i = 0; i < logs_count; i++) {
        storage_dir_read_index(app->storage, PATHLOGS, file_path, i);

        size_t position = furi_string_search_rchar(file_path, '/');
        if(position != FURI_STRING_FAILURE) {
            furi_string_set(file_name, file_path);
            furi_string_right(file_name, position + 1);
        }

        submenu_add_item(app->submenu, furi_string_get_cstr(file_name), i, submenu_callback, app);
    }

    furi_string_free(file_path);
    furi_string_free(file_name);

    submenu_set_selected_item(app->submenu, select_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuLogView);
}

bool app_scene_read_logs_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        select_index = submenu_get_selected_item(app->submenu);
        scene_manager_next_scene(app->scene_manager, app_scene_logs_options_scene);
        break;
    default:
        break;
    }

    return consumed;
}

void app_scene_read_logs_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
