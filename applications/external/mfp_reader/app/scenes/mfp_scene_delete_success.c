#include "../mfp_app.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <gui/modules/popup.h>
#include <furi.h>

typedef enum {
    DeleteSuccessEventTimeout = 0,
} DeleteSuccessCustomEvent;

static void delete_success_popup_cb(void* ctx) {
    MfpApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, DeleteSuccessEventTimeout);
}

void mfp_scene_delete_success_on_enter(void* ctx) {
    MfpApp* app = ctx;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Deleted", 64, 25, AlignCenter, AlignCenter);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, delete_success_popup_cb);
    popup_set_timeout(app->popup, 1500);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewPopup);
}

bool mfp_scene_delete_success_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;
    bool do_exit = false;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == DeleteSuccessEventTimeout) {
        do_exit = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        do_exit = true;
    }

    if(do_exit) {
        /* Refresh the Saved list: that scene's on_enter re-scans the
         * directory, which will now omit the deleted file. Skip back
         * past the file-viewer scenes and land directly on Saved. */
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, MfpSceneSaved)) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MfpSceneStart);
        }
        return true;
    }
    return false;
}

void mfp_scene_delete_success_on_exit(void* ctx) {
    MfpApp* app = ctx;
    popup_disable_timeout(app->popup);
    popup_set_callback(app->popup, NULL);
    popup_set_context(app->popup, NULL);
    popup_reset(app->popup);
}
