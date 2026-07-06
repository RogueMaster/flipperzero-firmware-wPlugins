#include "src/core/cfs_app_i.h"
#include "cfs_tag_writer_icons.h"

// "Saved Tags" opens Flipper's file browser so the user can navigate the
// filesystem and pick any .nfc dump — not just files in the app's own folder.
// The browser is rooted at the default save folder but Back walks up from there.
void cfs_scene_saved_tags_on_enter(void* context) {
    CfsApp* app = context;

    cfs_storage_ensure_dir(app->storage); // base_path must exist to browse it

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(
        &options, CFS_STORAGE_EXTENSION, &I_CfsTagFile_10x10);
    options.base_path = CFS_STORAGE_DIR;
    options.hide_dot_files = true;

    furi_string_set(app->selected_path, CFS_STORAGE_DIR); // start location
    bool picked =
        dialog_file_browser_show(app->dialogs, app->selected_path, app->selected_path, &options);

    CfsLoadedTag loaded;
    if(picked &&
       cfs_storage_load(app->storage, furi_string_get_cstr(app->selected_path), &loaded)) {
        app->tag_data = loaded.data;
        memcpy(app->uid, loaded.uid, loaded.uid_len);
        app->uid_len = loaded.uid_len;
        memcpy(app->tag_plain, loaded.plain, CFS_PLAINTEXT_SIZE);
        app->is_blank = false;
        app->has_tag_plain = true; // .nfc load recovers the exact decrypted payload
        scene_manager_next_scene(app->scene_manager, CfsSceneSavedEntry);
    } else {
        // Cancelled, or the file isn't a readable CFS tag — return to the menu.
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool cfs_scene_saved_tags_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cfs_scene_saved_tags_on_exit(void* context) {
    UNUSED(context);
}
