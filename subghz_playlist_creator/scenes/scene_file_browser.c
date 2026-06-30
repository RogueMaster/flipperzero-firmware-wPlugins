#include "../subghz_playlist_creator.h"
#include "scene_file_browser.h"
#include "scene_menu.h"
#include "scene_text_input.h"
#include <furi.h>
#include <gui/view_dispatcher.h>

// Remove global statics
typedef struct {
    SceneFileBrowserSelectCallback on_select;
} SceneFileBrowserContext;

static void file_browser_scene_callback(void* context) {
    SubGhzPlaylistCreator* app = context;
    if(furi_string_size(app->file_browser_result) > 0 && app->file_browser_select_cb) {
        app->file_browser_select_cb(app, furi_string_get_cstr(app->file_browser_result));
        app->file_browser_select_cb = NULL;
    } else {
        if(app->return_scene == ReturnScene_PlaylistEdit) {
            scene_playlist_edit_show(app);
        } else if(app->return_scene == ReturnScene_Menu) {
            scene_menu_show(app);
        } else if(app->return_scene == ReturnScene_TextInput) {
            scene_text_input_show(app);
        } else {
            scene_menu_show(app);
        }
    }
}

void scene_file_browser_select(
    SubGhzPlaylistCreator* app,
    const char* start_dir,
    const char* extension,
    SceneFileBrowserSelectCallback on_select
) {
    app->file_browser_select_cb = on_select;
    furi_string_set(app->file_browser_result, start_dir);
    file_browser_configure(
        app->file_browser,
        extension,
        start_dir,
        false,  // skip_assets
        true,   // hide_dot_files
        NULL,   // file_icon
        false   // hide_ext
    );
    file_browser_set_callback(app->file_browser, file_browser_scene_callback, app);
    file_browser_start(app->file_browser, app->file_browser_result);
    app->current_view = SubGhzPlaylistCreatorViewFileBrowser;
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPlaylistCreatorViewFileBrowser);
}

void scene_file_browser_show(SubGhzPlaylistCreator* app) {
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPlaylistCreatorViewFileBrowser);
}

// Add the definition for scene_file_browser_init_view
void scene_file_browser_init_view(SubGhzPlaylistCreator* app) {
    // The view is allocated in subghz_playlist_creator_alloc and added to the dispatcher there.
    // This function can remain empty for now.
    UNUSED(app);
} 