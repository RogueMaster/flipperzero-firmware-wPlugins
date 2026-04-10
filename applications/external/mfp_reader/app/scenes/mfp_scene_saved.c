#include "../mfp_app.h"
#include "../mfp_storage.h"
#include "mfp_scene_config.h"
#include <gui/scene_manager.h>
#include <storage/storage.h>
#include <string.h>

static void saved_submenu_cb(void* ctx, uint32_t index) {
    MfpApp* app = ctx;
    if(index >= app->saved_count) return;

    char path[128];
    snprintf(
        path,
        sizeof(path),
        MFP_APP_FOLDER "/%s" MFP_FILE_EXT,
        app->saved_names[index]);

    if(mfp_storage_load(app, path)) {
        /* Reuse the fresh-dump result view — loaded V2 files populate
         * version and sector_results so ReadAllResult can render them. */
        app->read_all_mode = true;
        scene_manager_next_scene(app->scene_manager, MfpSceneReadAllResult);
    }
}

void mfp_scene_saved_on_enter(void* ctx) {
    MfpApp* app = ctx;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Saved Cards");

    app->saved_count = 0;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, MFP_APP_FOLDER)) {
        FileInfo fi;
        char name[MFP_NAME_LEN];

        while(storage_dir_read(dir, &fi, name, sizeof(name)) &&
              app->saved_count < MFP_MAX_SAVED) {
            if(fi.flags & FSF_DIRECTORY) continue;

            size_t len = strlen(name);
            if(len <= 4 || strcmp(name + len - 4, MFP_FILE_EXT) != 0) continue;

            /* Strip extension for display */
            name[len - 4] = '\0';
            strlcpy(app->saved_names[app->saved_count], name, MFP_NAME_LEN);
            submenu_add_item(
                app->submenu,
                app->saved_names[app->saved_count],
                app->saved_count,
                saved_submenu_cb,
                app);
            app->saved_count++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);

    if(app->saved_count == 0) {
        submenu_add_item(app->submenu, "No saved cards", 0, NULL, NULL);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewSubmenu);
}

bool mfp_scene_saved_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void mfp_scene_saved_on_exit(void* ctx) {
    MfpApp* app = ctx;
    submenu_reset(app->submenu);
}
