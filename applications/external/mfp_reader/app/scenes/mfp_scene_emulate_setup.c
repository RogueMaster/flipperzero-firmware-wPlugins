#include "../mfp_app.h"
#include "../mfp_storage.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>

typedef enum {
    EmulateSetupWritable = 0,
    EmulateSetupReadOnly,
} EmulateSetupItem;

static void emulate_setup_cb(void* ctx, uint32_t index) {
    MfpApp* app = ctx;
    bool new_state = (index == EmulateSetupWritable);
    if(app->allow_overwrite != new_state) {
        app->allow_overwrite = new_state;
        /* Persist the flag to the dump file so the choice becomes
         * the new default for this card next time it's loaded.
         * Only do this if the dump was already saved to disk — we
         * never auto-create files behind the user's back. */
        if(app->save_path[0] != '\0' &&
           storage_file_exists(app->storage, app->save_path)) {
            mfp_storage_save_all(app);
        }
    }
    scene_manager_next_scene(app->scene_manager, MfpSceneEmulate);
}

void mfp_scene_emulate_setup_on_enter(void* ctx) {
    MfpApp* app = ctx;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Emulation mode");
    submenu_add_item(
        app->submenu,
        "Writable",
        EmulateSetupWritable,
        emulate_setup_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Read-only (discard)",
        EmulateSetupReadOnly,
        emulate_setup_cb,
        app);

    /* Pre-select the saved mode so user sees the current state */
    submenu_set_selected_item(
        app->submenu, app->allow_overwrite ? EmulateSetupWritable : EmulateSetupReadOnly);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewSubmenu);
}

bool mfp_scene_emulate_setup_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void mfp_scene_emulate_setup_on_exit(void* ctx) {
    MfpApp* app = ctx;
    submenu_reset(app->submenu);
}
