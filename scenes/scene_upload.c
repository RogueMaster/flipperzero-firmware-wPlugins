#include "../sigroam.h"
#include "../src/sr_upload_prompt.h"

#include <input/input.h>
#include <stdio.h>

enum {
    SigRoamUploadEventGo = 1,
};

static void sigroam_scene_upload_button(GuiButtonType result, InputType type, void* context);

static void sigroam_scene_upload_fill(SigRoamApp* app) {
    const SrUpInfo* u = &app->model.up;
    SrUploadPrompt prompt;
    uint8_t st = app->model.firmware.diag_state;
    bool seen = app->model.up_rev != 0u;

    if(sr_upload_prompt(
           app->model.firmware.diag_seen,
           st,
           app->model.up_rev,
           u->last_trans,
           u->reason,
           &prompt)) {
        snprintf(
            app->upload_text,
            sizeof(app->upload_text),
            "WiGLE upload\n"
            "q=%lu\n"
            "%s\n"
            "%s",
            seen ? (unsigned long)u->q : 0ul,
            prompt.line1,
            prompt.line2);
        return;
    }

    snprintf(
        app->upload_text,
        sizeof(app->upload_text),
        "WiGLE upload\n"
        "q=%lu\n"
        "%s",
        seen ? (unsigned long)u->q : 0ul,
        (app->model.firmware.diag_seen && st == 1u) ? "OK: stop+up" : "OK: upload");
}

static void sigroam_scene_upload_draw(SigRoamApp* app) {
    widget_reset(app->widget);
    sigroam_scene_upload_fill(app);
    widget_add_text_scroll_element(
        app->widget, 0, 0, SR_CANVAS_W, (uint8_t)(SR_CANVAS_H - 14), app->upload_text);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Upload", sigroam_scene_upload_button, app);
}

static void sigroam_scene_upload_button(GuiButtonType result, InputType type, void* context) {
    SigRoamApp* app = context;

    if(app == NULL || type != InputTypeShort || result != GuiButtonTypeCenter) {
        return;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, SigRoamUploadEventGo);
}

void sigroam_scene_upload_on_enter(void* context) {
    SigRoamApp* app = context;

    app->upload_up_rev_shown = app->model.up_rev;
    app->upload_go_retry = false;
    if(app->worker != NULL) {
        (void)sr_worker_send_cmd(app->worker, "uploadstatus\n");
    }
    sigroam_scene_upload_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewWidget);
}

bool sigroam_scene_upload_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == SigRoamUploadEventGo) {
        uint8_t st = app->model.firmware.diag_state;
        if(app->model.firmware.diag_seen && st == 5u) {
            app->upload_go_retry = false;
            return true;
        }
        if(app->worker != NULL && sr_worker_send_cmd(app->worker, "upload\n")) {
            app->upload_go_retry = false;
        } else {
            app->upload_go_retry = true;
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeTick) {
        if(app->upload_go_retry && app->worker != NULL) {
            uint8_t st = app->model.firmware.diag_state;
            if(app->model.firmware.diag_seen && st == 5u) {
                app->upload_go_retry = false;
            } else if(sr_worker_send_cmd(app->worker, "upload\n")) {
                app->upload_go_retry = false;
            }
        } else if((app->tick_n % 20u) == 0u && app->worker != NULL) {
            (void)sr_worker_send_cmd(app->worker, "uploadstatus\n");
        }
        if(app->upload_up_rev_shown != app->model.up_rev ||
           (app->tick_n % 10u) == 0u) {
            app->upload_up_rev_shown = app->model.up_rev;
            sigroam_scene_upload_draw(app);
        }
        return true;
    }
    return false;
}

void sigroam_scene_upload_on_exit(void* context) {
    SigRoamApp* app = context;
    widget_reset(app->widget);
}
