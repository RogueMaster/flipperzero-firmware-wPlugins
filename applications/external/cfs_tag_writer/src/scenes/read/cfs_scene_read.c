#include "src/core/cfs_app_i.h"

static void cfs_read_capture_result(CfsApp* app) {
    const CfsTagData* d = cfs_nfc_worker_get_tag_data(app->worker);
    app->tag_data = *d;
    memcpy(app->tag_plain, cfs_nfc_worker_get_read_plain(app->worker), CFS_PLAINTEXT_SIZE);
    app->has_tag_plain = true;
    size_t ul = 0;
    const uint8_t* u = cfs_nfc_worker_get_uid(app->worker, &ul);
    if(u) {
        if(ul > sizeof(app->uid)) ul = sizeof(app->uid);
        memcpy(app->uid, u, ul);
        app->uid_len = ul;
    }
}

static void cfs_read_show_error(CfsApp* app, const char* text) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "Read Failed", 64, 8, AlignCenter, AlignTop);
    popup_set_text(app->popup, text, 64, 34, AlignCenter, AlignCenter);
    notification_message(app->notifications, &sequence_error);
}

void cfs_scene_read_on_enter(void* context) {
    CfsApp* app = context;
    app->result_read_only = false; // this is a live read
    popup_reset(app->popup);
    popup_set_header(app->popup, "Reading", 64, 8, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Hold a CFS tag to\nthe back of Flipper", 64, 34, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewPopup);
    cfs_nfc_worker_start_read(app->worker);
}

bool cfs_scene_read_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case CfsCustomEventWorkerSuccess:
        cfs_read_capture_result(app);
        app->is_blank = false;
        notification_message(app->notifications, &sequence_success);
        scene_manager_next_scene(app->scene_manager, CfsSceneReadResult);
        return true;
    case CfsCustomEventWorkerBlankTag:
        cfs_read_capture_result(app);
        app->is_blank = true;
        scene_manager_next_scene(app->scene_manager, CfsSceneReadResult);
        return true;
    case CfsCustomEventWorkerNotPresent:
        cfs_read_show_error(app, "No tag detected.\nPress Back to retry.");
        return true;
    case CfsCustomEventWorkerWrongType:
        cfs_read_show_error(app, "Not a MIFARE\nClassic 1K tag.");
        return true;
    case CfsCustomEventWorkerAuthFailed:
        cfs_read_show_error(app, "Auth failed.\nNot a CFS tag?");
        return true;
    case CfsCustomEventWorkerReadFailed:
        cfs_read_show_error(app, "Decrypt/parse\nfailed.");
        return true;
    default:
        return false;
    }
}

void cfs_scene_read_on_exit(void* context) {
    CfsApp* app = context;
    cfs_nfc_worker_stop(app->worker);
    popup_reset(app->popup);
}
