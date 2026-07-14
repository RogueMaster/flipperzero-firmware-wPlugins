#include "src/core/cfs_app_i.h"

static void cfs_progress_prompt(CfsApp* app) {
    popup_reset(app->popup);
    char header[24];
    snprintf(header, sizeof(header), "Writing Tag %u", app->write_tag_index + 1);
    popup_set_header(app->popup, header, 64, 8, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Hold tag to the\nback of Flipper", 64, 34, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewPopup);
    if(app->write_exact) {
        // Clone: write the captured plaintext byte-for-byte (blocks 4-6 exact).
        cfs_nfc_worker_start_write_raw(app->worker, app->tag_plain);
    } else {
        cfs_nfc_worker_start_write(app->worker, &app->pending);
    }
}

// Fires when the user presses OK on the "present tag 2" prompt.
static void cfs_progress_tag2_callback(void* context) {
    CfsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonWriteTag2);
}

// Advance the sequential counter once per completed spool. This runs only on a
// final success: for a 2x write, tag 1's success is intercepted below (pause),
// so the bump happens after tag 2 — both sides share one serial and the counter
// moves by 1 per spool, not per tag.
static void cfs_progress_bump_serial(CfsApp* app) {
    if(app->settings.serial_mode == CfsSerialSequential) {
        app->settings.serial_counter++;
        cfs_settings_save(app->storage, &app->settings);
    }
}

void cfs_scene_write_progress_on_enter(void* context) {
    CfsApp* app = context;
    app->write_tag_index = 0;
    cfs_progress_prompt(app);
}

bool cfs_scene_write_progress_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case CfsCustomEventWorkerSuccess:
        if(app->dual_tag && app->write_tag_index == 0) {
            // Pause: let the user swap tags before writing the second one.
            notification_message(app->notifications, &sequence_success);
            popup_reset(app->popup);
            popup_set_header(app->popup, "Tag 1 Done", 64, 8, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Remove it, present\ntag 2, press OK",
                64,
                34,
                AlignCenter,
                AlignCenter);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, cfs_progress_tag2_callback);
        } else {
            cfs_progress_bump_serial(app);
            cfs_app_finish_op_popup(app, "Done", "Tag written OK", true);
        }
        return true;
    case CfsCustomEventButtonWriteTag2:
        app->write_tag_index = 1;
        popup_set_callback(app->popup, NULL);
        cfs_progress_prompt(app);
        return true;
    case CfsCustomEventWorkerNotPresent:
        cfs_app_finish_op_popup(app, "Write Failed", "No tag detected", false);
        return true;
    case CfsCustomEventWorkerWrongType:
        cfs_app_finish_op_popup(app, "Write Failed", "Not a 1K tag", false);
        return true;
    case CfsCustomEventWorkerWriteFailed:
        cfs_app_finish_op_popup(app, "Write Failed", "Could not write.\nTag locked?", false);
        return true;
    case CfsCustomEventWorkerVerifyFailed:
        cfs_app_finish_op_popup(app, "Verify Failed", "Read-back mismatch", false);
        return true;
    default:
        return false;
    }
}

void cfs_scene_write_progress_on_exit(void* context) {
    CfsApp* app = context;
    cfs_nfc_worker_stop(app->worker);
    popup_disable_timeout(app->popup);
    popup_set_callback(app->popup, NULL);
    popup_reset(app->popup);
}
