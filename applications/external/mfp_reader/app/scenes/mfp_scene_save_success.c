#include "../mfp_app.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <gui/modules/popup.h>
#include <furi.h>
#include <string.h>

typedef enum {
    SaveSuccessEventTimeout = 0,
} SaveSuccessCustomEvent;

static void save_success_popup_cb(void* ctx) {
    MfpApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, SaveSuccessEventTimeout);
}

void mfp_scene_save_success_on_enter(void* ctx) {
    MfpApp* app = ctx;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Saved", 64, 12, AlignCenter, AlignTop);

    /* Show the filename (basename of save_path) as body */
    const char* fname = strrchr(app->save_path, '/');
    fname = fname ? fname + 1 : app->save_path;
    popup_set_text(app->popup, fname, 64, 32, AlignCenter, AlignTop);

    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, save_success_popup_cb);
    popup_set_timeout(app->popup, 1500);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewPopup);
}

bool mfp_scene_save_success_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == SaveSuccessEventTimeout) {
        /* Return to the ReadAllResult view — that's where the user
         * came from via the Actions menu. Skip past Actions + Save
         * scenes in the stack. */
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, MfpSceneReadAllResult)) {
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        /* User pressed back before the timeout — same destination. */
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, MfpSceneReadAllResult)) {
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    }
    return false;
}

void mfp_scene_save_success_on_exit(void* ctx) {
    MfpApp* app = ctx;
    popup_disable_timeout(app->popup);
    popup_set_callback(app->popup, NULL);
    popup_set_context(app->popup, NULL);
    popup_reset(app->popup);
}
