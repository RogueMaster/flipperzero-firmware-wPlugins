#include "../sigroam.h"
#include "../src/sr_dialect.h"
#include "../src/sr_upload_prompt.h"
#include "../src/sr_view_fmt.h"

#include <input/input.h>
#include <stdio.h>

_Static_assert(
    (unsigned)SR_UPLOAD_INFO_PERIOD_TICKS == (unsigned)SR_QUAL_REFRESH_PERIOD_TICKS,
    "Upload info period must match Dash qual refresh");
_Static_assert(
    (unsigned)SR_UPLOAD_STATUS_PERIOD_TICKS*(unsigned)SR_TICK_PERIOD_MS == 2000u,
    "uploadstatus stays on the 2 s cadence");

enum {
    SigRoamUploadEventGo = 1,
};

static void sigroam_scene_upload_button(GuiButtonType result, InputType type, void* context);

static bool sigroam_scene_upload_hold(const SigRoamApp* app) {
    return sr_upload_prompt_hold(
        app->model.firmware.diag_seen, app->model.firmware.diag_state, app->model.up.reason);
}

/* Status wins the shared tick. Info only when this poll says so and the slot accepts it. */
static void sigroam_scene_upload_poll(SigRoamApp* app) {
    bool sigroam = sr_dialect_is_sigroam(&app->model.firmware);
    bool generic = sr_dialect_is_generic_marauder(&app->model.firmware);
    int poll;

    if(app->worker == NULL) {
        return;
    }
    poll = sr_upload_poll_cmd(
        app->tick_n, sigroam, generic, app->upload_ident_info_sent, app->upload_info_armed);
    if(poll == SR_UPLOAD_POLL_STATUS) {
        (void)sr_worker_send_cmd(app->worker, "uploadstatus\n");
        return;
    }
    if(poll != SR_UPLOAD_POLL_INFO) {
        return;
    }
    if(!sr_worker_send_cmd(app->worker, "info\n")) {
        return;
    }
    app->upload_info_armed = false;
    if(!sigroam) {
        app->upload_ident_info_sent = true;
    }
}

static bool sigroam_scene_upload_sd_dead(const SigRoamApp* app) {
    return sr_scan_ctl_sd_dead(
        app->model.qual_rev, app->model.qual.sd, app->model.qual_tick_ms, furi_get_tick());
}

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
           sigroam_scene_upload_sd_dead(app),
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
    app->upload_fw_rev_shown = app->model.firmware_rev;
    app->upload_qual_rev_shown = app->model.qual_rev;
    app->upload_go_retry = false;
    app->upload_info_armed = true;
    app->upload_ident_info_sent = false;
    if(app->worker != NULL) {
        (void)sr_worker_send_cmd(app->worker, "uploadstatus\n");
    }
    sigroam_scene_upload_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewWidget);
}

bool sigroam_scene_upload_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == SigRoamUploadEventGo) {
        if(sigroam_scene_upload_sd_dead(app) || sigroam_scene_upload_hold(app)) {
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
            if(sigroam_scene_upload_sd_dead(app) || sigroam_scene_upload_hold(app)) {
                app->upload_go_retry = false;
            } else if(sr_worker_send_cmd(app->worker, "upload\n")) {
                app->upload_go_retry = false;
            }
        } else {
            sigroam_scene_upload_poll(app);
        }
        if(app->upload_up_rev_shown != app->model.up_rev ||
           app->upload_fw_rev_shown != app->model.firmware_rev ||
           app->upload_qual_rev_shown != app->model.qual_rev || (app->tick_n % 10u) == 0u) {
            app->upload_up_rev_shown = app->model.up_rev;
            app->upload_fw_rev_shown = app->model.firmware_rev;
            app->upload_qual_rev_shown = app->model.qual_rev;
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
