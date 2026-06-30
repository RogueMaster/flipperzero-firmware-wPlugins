#include "../subghz_playlist_creator.h"
#include "scene_playlist_edit.h"
#include "scene_menu.h"
#include "scene_file_browser.h"
#include "scene_popup.h"
#include "scene_text_input.h"
#include <furi.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <string.h>
#include <gui/modules/dialog_ex.h>
#include <gui/view.h>
#include "scene_dialog.h"
#include <gui/modules/submenu.h>
#include <furi_hal.h>


#define MAX_PLAYLIST_LINES 128
#define MAX_FILENAME_LENGTH 128
#define SUBGHZ_DIRECTORY "/ext/subghz"
#define TAG "PlaylistEditScene"

// Dialog type for PlaylistEdit
typedef enum {
    PlaylistEditDialog_None = 0,
    PlaylistEditDialog_Discard,
    PlaylistEditDialog_Save,
    PlaylistEditDialog_Delete,
} PlaylistEditDialogType;

static PlaylistEditDialogType playlist_edit_dialog_type = PlaylistEditDialog_None;
static uint32_t playlist_edit_selected_index = 0;

// Add a static flag to indicate we should show menu after popup
static bool playlist_edit_show_menu_after_popup = false;

// Helper to read a line from file (since storage_file_read_line is not available)
// static bool file_read_line(File* file, char* buffer, size_t max_len) { ... }

// Dialog callback
static void edit_dialog_callback(DialogExResult result, void* context) {
    SubGhzPlaylistCreator* app = context;
    FURI_LOG_D(TAG, "edit_dialog_callback: dialog_type=%d, result=%d", (int)playlist_edit_dialog_type, (int)result);
    if(playlist_edit_dialog_type == PlaylistEditDialog_Discard) {
        if(result == DialogExResultLeft) {
            FURI_LOG_D(TAG, "Discard: DialogExResultLeft -> menu");
            app->playlist_modified = false;
            scene_menu_show(app);
        } else if(result == DialogExResultRight) {
            FURI_LOG_D(TAG, "Discard: DialogExResultRight -> stay in edit");
            scene_playlist_edit_show(app);
        } else {
            FURI_LOG_D(TAG, "Discard: Unknown result -> stay in edit");
            scene_playlist_edit_show(app);
        }
    } else if(playlist_edit_dialog_type == PlaylistEditDialog_Save) {
        if(result == DialogExResultRight) {
            FURI_LOG_D(TAG, "Save: DialogExResultRight -> save and menu");
            File* file = storage_file_alloc(app->storage);
            if(file && storage_file_open(file, furi_string_get_cstr(app->playlist_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                for(size_t i = 0; i < app->playlist_entry_count; ++i) {
                    storage_file_write(file, "sub: ", 5);
                    storage_file_write(file, app->playlist_entries[i], strlen(app->playlist_entries[i]));
                    storage_file_write(file, "\n", 1);
                }
                storage_file_close(file);
            }
            if(file) storage_file_free(file);
            app->playlist_modified = false;
            for(size_t i = 0; i < app->playlist_entry_count; ++i) {
                free(app->playlist_entries[i]);
            }
            free(app->playlist_entries);
            app->playlist_entries = NULL;
            app->playlist_entry_count = 0;
            app->playlist_entry_capacity = 0;
            playlist_edit_show_menu_after_popup = true;
            scene_popup_show(app, "Success", "Playlist saved!");
            return;
        } else if(result == DialogExResultLeft) {
            FURI_LOG_D(TAG, "Save: DialogExResultLeft -> stay in edit");
            scene_playlist_edit_show(app);
        } else {
            FURI_LOG_D(TAG, "Save: Unknown result -> stay in edit");
            scene_playlist_edit_show(app);
        }
    } else if(playlist_edit_dialog_type == PlaylistEditDialog_Delete) {
        if(result == DialogExResultRight) {
            FURI_LOG_D(TAG, "Delete: DialogExResultRight -> remove entry");
            if(playlist_edit_selected_index < app->playlist_entry_count) {
                free(app->playlist_entries[playlist_edit_selected_index]);
                for(size_t i = playlist_edit_selected_index; i + 1 < app->playlist_entry_count; ++i) {
                    app->playlist_entries[i] = app->playlist_entries[i + 1];
                }
                app->playlist_entry_count--;
                app->playlist_modified = true;
                if(playlist_edit_selected_index >= app->playlist_entry_count) playlist_edit_selected_index = app->playlist_entry_count ? app->playlist_entry_count - 1 : 0;
            }
            scene_playlist_edit_show(app);
        } else {
            FURI_LOG_D(TAG, "Delete: Not right -> stay in edit");
            scene_playlist_edit_show(app);
        }
    }
    playlist_edit_dialog_type = PlaylistEditDialog_None;
}

// Move the full definition of on_add_file_selected here:
static void on_add_file_selected(SubGhzPlaylistCreator* app, const char* path) {
    if(path && strlen(path)) {
        if(app->playlist_entry_count == app->playlist_entry_capacity) {
            app->playlist_entry_capacity = app->playlist_entry_capacity ? app->playlist_entry_capacity * 2 : 8;
            app->playlist_entries = realloc(app->playlist_entries, app->playlist_entry_capacity * sizeof(char*));
        }
        app->playlist_entries[app->playlist_entry_count++] = strdup(path);
        app->playlist_modified = true;
    }
    scene_playlist_edit_show(app);
}

// Move the full definition of playlist_edit_submenu_callback here:
static void playlist_edit_submenu_callback(void* context, uint32_t index) {
    SubGhzPlaylistCreator* app = context;
    FURI_LOG_D(TAG, "playlist_edit_submenu_callback: index=%u, entry_count=%u", (unsigned int)index, (unsigned int)app->playlist_entry_count);
    if(index == app->playlist_entry_count) {
        FURI_LOG_D(TAG, "[+] Add file selected");
        app->return_scene = ReturnScene_PlaylistEdit;
        scene_file_browser_select(app, SUBGHZ_DIRECTORY, ".sub", on_add_file_selected);
    } else if(index == app->playlist_entry_count + 1) {
        FURI_LOG_D(TAG, "Save playlist selected, showing dialog");
        scene_dialog_show_custom(
            app,
            "Save playlist?",
            "Are you sure you want to save playlist?",
            "Cancel",
            "Save",
            edit_dialog_callback,
            app
        );
        playlist_edit_dialog_type = PlaylistEditDialog_Save;
    } else {
        FURI_LOG_D(TAG, "Entry selected for delete, showing dialog");
        scene_dialog_show_custom(
            app,
            "Delete entry?",
            "Remove file from playlist?",
            "Cancel",
            "Delete",
            edit_dialog_callback,
            app
        );
        playlist_edit_dialog_type = PlaylistEditDialog_Delete;
    }
}

// Show PlaylistEdit scene using SubMenu
void scene_playlist_edit_show(SubGhzPlaylistCreator* app) {
    if(playlist_edit_show_menu_after_popup) {
        playlist_edit_show_menu_after_popup = false;
        scene_menu_show(app);
        return;
    }
    app->current_view = SubGhzPlaylistCreatorViewPlaylistEdit;
    submenu_reset(app->playlist_edit_submenu);
    // Set header to playlist name with extension
    submenu_set_header(app->playlist_edit_submenu, furi_string_get_cstr(app->playlist_name));
    // Add playlist entries
    for(size_t i = 0; i < app->playlist_entry_count; ++i) {
        const char* fname = strrchr(app->playlist_entries[i], '/');
        fname = fname ? fname + 1 : app->playlist_entries[i];
        submenu_add_item(app->playlist_edit_submenu, fname, i, playlist_edit_submenu_callback, app);
    }
    // Add '[+] Add file' as next-to-last item
    submenu_add_item(app->playlist_edit_submenu, "[+] Add file", app->playlist_entry_count, playlist_edit_submenu_callback, app);
    // Add 'Save playlist' as last item
    submenu_add_item(app->playlist_edit_submenu, "[s] Save playlist", app->playlist_entry_count + 1, playlist_edit_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPlaylistCreatorViewPlaylistEdit);
}

void scene_playlist_edit_init_view(SubGhzPlaylistCreator* app) {
    // No need to allocate VariableItemList, use SubMenu
    // SubMenu is already allocated in app->playlist_edit_submenu
    // Just ensure view is added for PlaylistEdit (done in app alloc)
    UNUSED(app);
}

// Custom back event handler for PlaylistEdit
bool scene_playlist_edit_back_event_callback(void* context) {
    SubGhzPlaylistCreator* app = context;
    // Show discard dialog instead of going to main menu
    scene_dialog_show_custom(
        app,
        "Discard changes?",
        "Are you sure you want to discard playlist?",
        "Discard",
        "Keep",
        edit_dialog_callback,
        app
    );
    playlist_edit_dialog_type = PlaylistEditDialog_Discard;
    return false; // Prevent view dispatcher from popping the view
}