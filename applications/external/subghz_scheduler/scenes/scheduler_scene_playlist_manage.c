#include "helpers/scheduler_settings_io.h"
#include "src/scheduler_app_i.h"

#include <gui/modules/validators.h>
#include <string.h>

typedef enum {
    PlaylistManageRename = 100,
    PlaylistManageDelete,
    PlaylistRenameDone,
} PlaylistManageEvent;

static bool playlist_path_is_managed(const char* path) {
    const char* folder = SCHEDULER_PLAYLIST_FOLDER "/";
    const size_t folder_len = strlen(folder);
    const size_t path_len = strlen(path);
    return path_len > folder_len + 4 && strncmp(path, folder, folder_len) == 0 &&
           strchr(path + folder_len, '/') == NULL && strcmp(path + path_len - 4, ".txt") == 0;
}

static void playlist_manage_callback(void* context, uint32_t index) {
    SchedulerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void scheduler_scene_playlist_pick_on_enter(void* context) {
    SchedulerApp* app = context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SCHEDULER_PLAYLIST_FOLDER);

    submenu_reset(app->menu);
    submenu_set_header(app->menu, "Saved Playlists");
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, SCHEDULER_PLAYLIST_FOLDER)) {
        FileInfo info;
        char name[256];
        while(app->managed_playlist_choice_count < SCHEDULER_MANAGED_PLAYLIST_MAX_ITEMS &&
              storage_dir_read(dir, &info, name, sizeof(name))) {
            const size_t len = strlen(name);
            if(file_info_is_dir(&info) || len < 5 || strcmp(name + len - 4, ".txt") != 0) {
                continue;
            }
            FuriString* path = furi_string_alloc_printf("%s/%s", SCHEDULER_PLAYLIST_FOLDER, name);
            app->managed_playlist_choices[app->managed_playlist_choice_count] = path;
            submenu_add_item(
                app->menu,
                path_basename(furi_string_get_cstr(path)),
                app->managed_playlist_choice_count,
                playlist_manage_callback,
                app);
            app->managed_playlist_choice_count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    if(!app->managed_playlist_choice_count) {
        submenu_add_item(app->menu, "No playlists found", 100, playlist_manage_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewMenu);
}

bool scheduler_scene_playlist_pick_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= app->managed_playlist_choice_count) return true;
    furi_string_set(app->managed_playlist_path, app->managed_playlist_choices[event.event]);
    if(playlist_path_is_managed(furi_string_get_cstr(app->managed_playlist_path))) {
        scene_manager_next_scene(app->scene_manager, SchedulerScenePlaylistManage);
    }
    return true;
}

void scheduler_scene_playlist_pick_on_exit(void* context) {
    SchedulerApp* app = context;
    submenu_reset(app->menu);
    for(uint8_t i = 0; i < app->managed_playlist_choice_count; i++) {
        furi_string_free(app->managed_playlist_choices[i]);
        app->managed_playlist_choices[i] = NULL;
    }
    app->managed_playlist_choice_count = 0;
}

void scheduler_scene_playlist_manage_on_enter(void* context) {
    SchedulerApp* app = context;
    submenu_reset(app->menu);
    submenu_set_header(app->menu, path_basename(furi_string_get_cstr(app->managed_playlist_path)));
    submenu_add_item(app->menu, "Rename", PlaylistManageRename, playlist_manage_callback, app);
    submenu_add_item(app->menu, "Delete", PlaylistManageDelete, playlist_manage_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewMenu);
}

bool scheduler_scene_playlist_manage_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == PlaylistManageRename) {
        scene_manager_next_scene(app->scene_manager, SchedulerScenePlaylistRename);
        return true;
    }
    if(event.event == PlaylistManageDelete) {
        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_header(message, "Delete playlist?", 64, 4, AlignCenter, AlignTop);
        dialog_message_set_text(
            message,
            path_basename(furi_string_get_cstr(app->managed_playlist_path)),
            64,
            27,
            AlignCenter,
            AlignCenter);
        dialog_message_set_buttons(message, "Cancel", NULL, "Delete");
        DialogMessageButton choice = dialog_message_show(app->dialogs, message);
        dialog_message_free(message);

        if(choice != DialogMessageButtonRight) return true;

        Storage* storage = furi_record_open(RECORD_STORAGE);
        FS_Error result =
            storage_common_remove(storage, furi_string_get_cstr(app->managed_playlist_path));
        furi_record_close(RECORD_STORAGE);
        if(result != FSE_OK) {
            dialog_message_show_storage_error(app->dialogs, "Failed to delete playlist");
            return true;
        }

        if(furi_string_equal(app->tx_file_path, app->managed_playlist_path)) {
            furi_string_reset(app->tx_file_path);
            scheduler_set_file(app->scheduler, "", 0);
        }
        furi_string_reset(app->managed_playlist_path);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SchedulerScenePlaylistPick);
        return true;
    }
    return false;
}

void scheduler_scene_playlist_manage_on_exit(void* context) {
    SchedulerApp* app = context;
    submenu_reset(app->menu);
}

static void playlist_rename_callback(void* context) {
    SchedulerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PlaylistRenameDone);
}

void scheduler_scene_playlist_rename_on_enter(void* context) {
    SchedulerApp* app = context;
    const char* name = path_basename(furi_string_get_cstr(app->managed_playlist_path));
    const size_t name_len = strlen(name);
    const size_t base_len = name_len >= 4 ? name_len - 4 : 0;
    snprintf(app->managed_name_tmp, sizeof(app->managed_name_tmp), "%.*s", (int)base_len, name);

    text_input_set_header_text(app->text_input, "Rename playlist");
    text_input_set_result_callback(
        app->text_input,
        playlist_rename_callback,
        app,
        app->managed_name_tmp,
        sizeof(app->managed_name_tmp),
        false);

    ValidatorIsFile* validator =
        validator_is_file_alloc_init(SCHEDULER_PLAYLIST_FOLDER, ".txt", app->managed_name_tmp);
    text_input_set_validator(app->text_input, validator_is_file_callback, validator);
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewTextInput);
}

bool scheduler_scene_playlist_rename_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != PlaylistRenameDone)
        return false;
    if(!app->managed_name_tmp[0] || strchr(app->managed_name_tmp, '/')) return true;

    FuriString* new_path =
        furi_string_alloc_printf("%s/%s.txt", SCHEDULER_PLAYLIST_FOLDER, app->managed_name_tmp);
    const char* old_path = furi_string_get_cstr(app->managed_playlist_path);
    const char* destination = furi_string_get_cstr(new_path);

    if(strcmp(old_path, destination) != 0) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        bool exists = storage_common_stat(storage, destination, NULL) == FSE_OK;
        FS_Error result = exists ? FSE_EXIST :
                                   storage_common_rename(storage, old_path, destination);
        furi_record_close(RECORD_STORAGE);

        if(result != FSE_OK) {
            dialog_message_show_storage_error(
                app->dialogs, exists ? "Name already exists" : "Failed to rename playlist");
            furi_string_free(new_path);
            return true;
        }

        if(furi_string_equal(app->tx_file_path, app->managed_playlist_path)) {
            furi_string_set(app->tx_file_path, new_path);
            scheduler_set_file(
                app->scheduler, destination, scheduler_get_list_count(app->scheduler));
        }
        furi_string_set(app->managed_playlist_path, new_path);
    }

    furi_string_free(new_path);
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void scheduler_scene_playlist_rename_on_exit(void* context) {
    SchedulerApp* app = context;
    void* validator = text_input_get_validator_callback_context(app->text_input);
    text_input_set_validator(app->text_input, NULL, NULL);
    if(validator) validator_is_file_free(validator);
    text_input_reset(app->text_input);
}
