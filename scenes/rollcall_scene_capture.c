#include "../rollcall_i.h"

/* Raw demodulator transitions per 100ms tick that count as "the radio is
 * hearing something". A keyed OOK fob runs a couple of hundred edges per tick;
 * an empty band idles well below this. */
#define RC_ACTIVITY_EDGES 120

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

    /* Scene state carries the last raw-edge total so the tick can turn it into
     * a rate without needing a field on the app. */
    scene_manager_set_scene_state(app->scene_manager, RollCallSceneCapture, 0);

    rc_radio_configure(
        app->radio,
        rc_bands[app->settings.band_idx].frequency,
        rc_mods[app->settings.mod_idx].preset);
    rc_radio_set_press_gap(app->radio, rc_gaps[app->settings.gap_idx].ms);

    capture_view_reset(app->capture_view);
    capture_view_set_config(
        app->capture_view,
        rc_bands[app->settings.band_idx].label,
        rc_mods[app->settings.mod_idx].label,
        app->settings.target);
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
            const RcCapture* last = n > 0 ? &app->captures[n - 1] : NULL;
            capture_view_set_progress(
                app->capture_view,
                n,
                last ? last->protocol : NULL,
                last ? last->cls : RcCodeUnknown,
                last ? last->bits : 0);
            rollcall_notify_capture(app);

            if(n >= app->settings.target) {
                rollcall_capture_finish(app);
            }
            consumed = true;
        } else if(event.event == RollCallCustomEventFinish) {
            rollcall_capture_finish(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        capture_view_tick(app->capture_view);

        /* Live diagnostics: carrier strength, plus whether the demodulator is
         * producing edges at all. Together they separate "wrong frequency"
         * from "right frequency, protocol we cannot decode". */
        uint32_t edges = rc_radio_edges(app->radio);
        uint32_t previous =
            scene_manager_get_scene_state(app->scene_manager, RollCallSceneCapture);
        scene_manager_set_scene_state(app->scene_manager, RollCallSceneCapture, edges);

        bool active = (edges - previous) > RC_ACTIVITY_EDGES;
        float rssi = rc_radio_rssi(app->radio);
        int8_t dbm = rssi < -127.0f ? -127 : (int8_t)rssi;
        capture_view_set_signal(app->capture_view, dbm, active);

        consumed = true;
    }
    return consumed;
}

void rollcall_scene_capture_on_exit(void* context) {
    RollCallApp* app = context;
    rc_radio_stop(app->radio);
}
