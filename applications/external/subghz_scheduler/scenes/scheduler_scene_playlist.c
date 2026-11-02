#include "src/scheduler_app_i.h"

#include <types.h>
#include <string.h>

typedef enum {
    PlaylistActionAdd = SCHEDULER_PLAYLIST_MAX_ITEMS,
    PlaylistActionWait,
    PlaylistActionRemoveLast,
    PlaylistActionClear,
    PlaylistActionSave,
} PlaylistAction;

static void playlist_menu_callback(void* context, uint32_t index) {
    SchedulerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void playlist_rebuild(SchedulerApp* app) {
    submenu_reset(app->menu);
    submenu_set_header(app->menu, "Build Playlist");

    char label[80];
    for(uint8_t i = 0; i < app->playlist_count; i++) {
        const char* path = furi_string_get_cstr(app->playlist_items[i]);
        if(!strncmp(path, "time:", 5)) {
            snprintf(label, sizeof(label), "%u. Wait %ss", (unsigned)(i + 1), path + 6);
        } else {
            const char* name = strrchr(path, '/');
            snprintf(label, sizeof(label), "%u. %s", (unsigned)(i + 1), name ? name + 1 : path);
        }
        submenu_add_item(app->menu, label, i, playlist_menu_callback, app);
    }

    submenu_add_item(app->menu, "Add signal...", PlaylistActionAdd, playlist_menu_callback, app);
    submenu_add_item(app->menu, "Add wait...", PlaylistActionWait, playlist_menu_callback, app);
    if(app->playlist_count) {
        submenu_add_item(
            app->menu, "Remove last", PlaylistActionRemoveLast, playlist_menu_callback, app);
        submenu_add_item(
            app->menu, "Clear list", PlaylistActionClear, playlist_menu_callback, app);
        submenu_add_item(
            app->menu, "Save playlist...", PlaylistActionSave, playlist_menu_callback, app);
    }
    submenu_set_selected_item(app->menu, PlaylistActionAdd);
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewMenu);
}

static void playlist_add_signal(SchedulerApp* app) {
    if(app->playlist_count == SCHEDULER_PLAYLIST_MAX_ITEMS) {
        dialog_message_show_storage_error(app->dialogs, "Playlist is full");
        return;
    }

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".sub", &I_sub1_10px);
    options.skip_assets = true;
    options.hide_ext = false;
    options.base_path = SUBGHZ_APP_FOLDER;

    FuriString* path = furi_string_alloc_set_str(SUBGHZ_APP_FOLDER);
    if(dialog_file_browser_show(app->dialogs, path, path, &options)) {
        app->playlist_items[app->playlist_count++] = path;
    } else {
        furi_string_free(path);
    }
    playlist_rebuild(app);
}

static void playlist_remove_last(SchedulerApp* app) {
    if(!app->playlist_count) return;
    furi_string_free(app->playlist_items[--app->playlist_count]);
    app->playlist_items[app->playlist_count] = NULL;
}

static bool playlist_can_save(SchedulerApp* app) {
    if(!app->playlist_count) {
        dialog_message_show_storage_error(app->dialogs, "Add a signal first");
        return false;
    }
    const char* last = furi_string_get_cstr(app->playlist_items[app->playlist_count - 1]);
    if(!strncmp(last, "time:", 5)) {
        dialog_message_show_storage_error(app->dialogs, "Add a signal after wait");
        return false;
    }
    return true;
}

void scheduler_scene_playlist_on_enter(void* context) {
    SchedulerApp* app = context;
    playlist_rebuild(app);
}

bool scheduler_scene_playlist_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case PlaylistActionAdd:
        playlist_add_signal(app);
        return true;
    case PlaylistActionRemoveLast:
        playlist_remove_last(app);
        playlist_rebuild(app);
        return true;
    case PlaylistActionWait:
        if(app->playlist_count == SCHEDULER_PLAYLIST_MAX_ITEMS) {
            dialog_message_show_storage_error(app->dialogs, "Playlist is full");
        } else {
            scene_manager_next_scene(app->scene_manager, SchedulerScenePlaylistWait);
        }
        return true;
    case PlaylistActionClear:
        while(app->playlist_count) {
            playlist_remove_last(app);
        }
        playlist_rebuild(app);
        return true;
    case PlaylistActionSave:
        if(playlist_can_save(app)) {
            scene_manager_next_scene(app->scene_manager, SchedulerScenePlaylistName);
        }
        return true;
    default:
        return event.event < SCHEDULER_PLAYLIST_MAX_ITEMS;
    }
}

void scheduler_scene_playlist_on_exit(void* context) {
    SchedulerApp* app = context;
    submenu_reset(app->menu);
}
