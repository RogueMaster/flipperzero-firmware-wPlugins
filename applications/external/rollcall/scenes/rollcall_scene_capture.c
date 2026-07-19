#include "../rollcall_i.h"

static void rollcall_scene_capture_view_cb(void* context, CaptureEvent event) {
    RollCallApp* app = context;
    if(event == CaptureEventFinish) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RollCallCustomEventFinish);
    }
}

/* Stop listening, evaluate what we captured, and hand off to the verdict. */
static void rollcall_capture_finish(RollCallApp* app) {
    rc_radio_stop(app->radio);
    app->capture_count = rc_radio_snapshot(app->radio, app->captures, RC_MAX_CAPTURES);
    rc_analyze(app->captures, app->capture_count, &app->verdict);
    app->have_verdict = true;
    rollcall_notify_verdict(app, app->verdict.health);
    scene_manager_next_scene(app->scene_manager, RollCallSceneVerdict);
}

void rollcall_scene_capture_on_enter(void* context) {
    RollCallApp* app = context;

    rc_radio_configure(
        app->radio, rc_bands[app->band_idx].frequency, rc_mods[app->mod_idx].preset);

    capture_view_reset(app->capture_view);
    capture_view_set_config(
        app->capture_view, rc_bands[app->band_idx].label, rc_mods[app->mod_idx].label, app->target);
    capture_view_set_callback(app->capture_view, rollcall_scene_capture_view_cb, app);

    rc_radio_start(app->radio);
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewCapture);
}

bool rollcall_scene_capture_on_event(void* context, SceneManagerEvent event) {
    RollCallApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RollCallCustomEventCapture) {
            uint8_t n = rc_radio_snapshot(app->radio, app->captures, RC_MAX_CAPTURES);
            app->capture_count = n;
            const char* proto = n > 0 ? app->captures[n - 1].protocol : NULL;
            RcCodeClass cls = n > 0 ? app->captures[n - 1].cls : RcCodeUnknown;
            capture_view_set_progress(app->capture_view, n, proto, cls);
            rollcall_notify_capture(app);

            if(n >= app->target) {
                rollcall_capture_finish(app);
            }
            consumed = true;
        } else if(event.event == RollCallCustomEventFinish) {
            rollcall_capture_finish(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        capture_view_tick(app->capture_view);
        consumed = true;
    }
    return consumed;
}

void rollcall_scene_capture_on_exit(void* context) {
    RollCallApp* app = context;
    rc_radio_stop(app->radio);
}
