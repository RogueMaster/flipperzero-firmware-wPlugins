#include "src/core/cfs_app_i.h"
#include "src/core/cfs_ui.h"
#include "src/data/cfs_shades.h"

// The color being written: a clone writes the loaded tag as-is.
static const CfsTagData* cfs_progress_color(CfsApp* app) {
    return app->write_exact ? &app->tag_data : &app->pending;
}

static void cfs_progress_prompt(CfsApp* app) {
    char title[24];
    if(app->dual_tag) {
        snprintf(title, sizeof(title), "Writing tag %u", app->write_tag_index + 1);
    } else {
        snprintf(title, sizeof(title), "Writing tag");
    }
    cfs_ui_show_working(
        app, title, "Hold the tag to\nthe back of\nyour Flipper", cfs_progress_color(app), true);
    if(app->write_exact) {
        // Clone: write the captured plaintext byte-for-byte (blocks 4-6 exact).
        cfs_nfc_worker_start_write_raw(app->worker, app->tag_plain);
    } else {
        cfs_nfc_worker_start_write(app->worker, &app->pending);
    }
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

static void cfs_progress_done(CfsApp* app) {
    const CfsTagData* d = cfs_progress_color(app);
    char name[32];
    char text[64];
    cfs_ui_material_name(d, name, sizeof(name));
    snprintf(
        text,
        sizeof(text),
        "%s\n%s\n%s  %u m",
        name,
        cfs_color_nearest_name(d->color_r, d->color_g, d->color_b),
        cfs_weight_label(cfs_nearest_weight_index(app->settings.weight_len, d->length_m)),
        d->length_m);
    cfs_ui_finish_op(app, true, "Tag written", text);
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
            CfsStatusModel status = {.kind = CfsStatusOk, .button_center = "Next"};
            snprintf(status.title, sizeof(status.title), "Tag 1 done");
            snprintf(
                status.text, sizeof(status.text), "Take it off, put\ntag 2 on, then\npress OK");
            cfs_screen_show_status(app->screen, &status, CfsCustomEventButtonWriteTag2, 0, 0);
        } else {
            cfs_progress_bump_serial(app);
            cfs_progress_done(app);
        }
        return true;
    case CfsCustomEventButtonWriteTag2:
        app->write_tag_index = 1;
        cfs_progress_prompt(app);
        return true;
    case CfsCustomEventButtonRetry:
        // Same tag again (tag 2 stays tag 2 on a 2x write).
        cfs_progress_prompt(app);
        return true;
    case CfsCustomEventScreenDone:
        // From a template: back to the template list, ready for the next tag.
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, CfsSceneTemplates)) {
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, CfsSceneStart);
        }
        return true;
    case CfsCustomEventWorkerNotPresent:
        cfs_ui_finish_op(app, false, "Write failed", "No tag found.\nHold it flat on\nthe back.");
        return true;
    case CfsCustomEventWorkerWrongType:
        cfs_ui_finish_op(app, false, "Write failed", "Not a MIFARE\nClassic 1K tag.");
        return true;
    case CfsCustomEventWorkerWriteFailed:
        cfs_ui_finish_op(app, false, "Write failed", "Could not write.\nTag locked?");
        return true;
    case CfsCustomEventWorkerVerifyFailed:
        cfs_ui_finish_op(app, false, "Verify failed", "Read-back did\nnot match.");
        return true;
    default:
        return false;
    }
}

void cfs_scene_write_progress_on_exit(void* context) {
    CfsApp* app = context;
    cfs_nfc_worker_stop(app->worker);
    cfs_screen_reset(app->screen);
}
