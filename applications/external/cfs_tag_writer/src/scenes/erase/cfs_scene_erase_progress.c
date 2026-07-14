#include "src/core/cfs_app_i.h"

void cfs_scene_erase_progress_on_enter(void* context) {
    CfsApp* app = context;
    popup_reset(app->popup);
    popup_set_header(app->popup, "Erasing", 64, 8, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Hold tag to the\nback of Flipper", 64, 34, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewPopup);
    cfs_nfc_worker_start_erase(app->worker);
}

bool cfs_scene_erase_progress_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case CfsCustomEventWorkerSuccess:
        cfs_app_finish_op_popup(app, "Done", "Tag erased", true);
        return true;
    case CfsCustomEventWorkerNotPresent:
        cfs_app_finish_op_popup(app, "Erase Failed", "No tag detected", false);
        return true;
    case CfsCustomEventWorkerWrongType:
        cfs_app_finish_op_popup(app, "Erase Failed", "Not a 1K tag", false);
        return true;
    case CfsCustomEventWorkerWriteFailed:
        cfs_app_finish_op_popup(app, "Erase Failed", "Could not write.\nTag locked?", false);
        return true;
    case CfsCustomEventWorkerVerifyFailed:
        cfs_app_finish_op_popup(app, "Verify Failed", "Read-back mismatch", false);
        return true;
    default:
        return false;
    }
}

void cfs_scene_erase_progress_on_exit(void* context) {
    CfsApp* app = context;
    cfs_nfc_worker_stop(app->worker);
    popup_disable_timeout(app->popup);
    popup_set_callback(app->popup, NULL);
    popup_reset(app->popup);
}
