#include "../../rf_studio_i.h"

/*
 * Guided capture (Addendum 001 Sections 2.1 and 4).
 * Starts with frequency discovery unless a frequency is already known, then listens, and moves to
 * the result screen when a signal is decoded or a raw recording completes.
 */

static void rfs_capture_show_error(RfStudioApp* app, const char* reason) {
    rfs_capture_view_set_text(
        app->capture_view, "Can't Listen", reason, "Press Left for options,", "or Back to cancel.");
    rfs_capture_view_set_rssi(app->capture_view, false, 0, 0);
    rfs_capture_view_set_buttons(app->capture_view, "Options", NULL, "Retry");
}

static void rfs_capture_show_listening(RfStudioApp* app, const char* hint) {
    char line1[48];
    char mhz[16];
    rfs_format_mhz(rfs_capture_frequency(app->capture), mhz, sizeof(mhz));
    snprintf(line1, sizeof(line1), "%s MHz  %s", mhz, app->capture_preset);
    char line3[32];
    snprintf(
        line3,
        sizeof(line3),
        "Radio: %s",
        rfs_radio_device_label(rfs_capture_device(app->capture)));
    rfs_capture_view_set_text(app->capture_view, "Listening", line1, hint, line3);
    rfs_capture_view_set_buttons(app->capture_view, "Options", NULL, "Auto");
}

static void rfs_capture_begin(RfStudioApp* app) {
    RfsDeviceId device = RfsDeviceInternal;
    RfsRadioResult result = rfs_radio_resolve(
        app->radio, app->capture_radio, app->capture_frequency, false, &device, NULL);
    if(result == RfsRadioOk) {
        if(app->capture_frequency == 0) {
            result = rfs_capture_start_discovery(
                app->capture, device, app->settings.discover_scope, app->settings.rssi_threshold);
            if(result == RfsRadioOk) {
                rfs_capture_view_set_text(
                    app->capture_view,
                    "Searching for RF...",
                    "Press a button on",
                    "your original remote.",
                    "");
                rfs_capture_view_set_buttons(app->capture_view, "Options", NULL, NULL);
            }
        } else {
            result = rfs_capture_start_listen(
                app->capture,
                device,
                app->capture_frequency,
                app->capture_preset,
                app->settings.rssi_threshold);
            if(result == RfsRadioOk) {
                rfs_capture_show_listening(app, "Press the remote button.");
            }
        }
    }
    if(result != RfsRadioOk) {
        rfs_diag_error(app->diag, "Capture: %s", rfs_radio_result_text(result));
        rfs_capture_show_error(app, rfs_radio_result_text(result));
    } else {
        rfs_capture_view_set_rssi(app->capture_view, true, -200.0f, app->settings.rssi_threshold);
    }
}

void rfs_scene_capture_on_enter(void* context) {
    RfStudioApp* app = context;
    rfs_capture_begin(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewCapture);
}

static void rfs_capture_update(RfStudioApp* app) {
    RfsCaptureState state = rfs_capture_state(app->capture);
    if(state == RfsCaptureStateDiscovering) {
        uint32_t sweep = rfs_capture_sweep_frequency(app->capture);
        if(sweep) {
            char mhz[16];
            char line[40];
            rfs_format_mhz(sweep, mhz, sizeof(mhz));
            snprintf(line, sizeof(line), "Scanning %s MHz", mhz);
            rfs_capture_view_set_text(
                app->capture_view,
                "Searching for RF...",
                "Press a button on",
                "your original remote.",
                line);
        }
    } else if(state == RfsCaptureStateListening) {
        size_t samples = rfs_capture_raw_samples(app->capture);
        if(samples > 0) {
            char line[32];
            snprintf(line, sizeof(line), "Recording: %u samples", (unsigned)samples);
            rfs_capture_show_listening(app, line);
            rfs_capture_view_set_buttons(app->capture_view, "Options", "Keep", "Auto");
        }
    } else {
        return;
    }
    rfs_capture_view_set_rssi(
        app->capture_view, true, rfs_capture_rssi(app->capture), app->settings.rssi_threshold);
}

static void rfs_capture_to_result(RfStudioApp* app) {
    rfs_capture_stop(app->capture);
    if(rfs_capture_result(app->capture)->valid) {
        notification_message(app->notifications, &sequence_success);
        scene_manager_next_scene(app->scene_manager, RfsSceneCaptureResult);
    } else {
        rfs_capture_begin(app);
    }
}

bool rfs_scene_capture_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        rfs_capture_tick(app->capture);
        rfs_capture_update(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        rfs_app_exit_capture(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case RfsEventCaptureFound: {
        /* Lock onto the discovered frequency and listen there (CAP-ADD-001). */
        app->capture_frequency = rfs_capture_frequency(app->capture);
        RfsRadioResult result = rfs_capture_start_listen(
            app->capture,
            rfs_capture_device(app->capture),
            app->capture_frequency,
            app->capture_preset,
            app->settings.rssi_threshold);
        if(result == RfsRadioOk) {
            notification_message(app->notifications, &sequence_single_vibro);
            char mhz[16];
            char title[40];
            rfs_format_mhz(app->capture_frequency, mhz, sizeof(mhz));
            snprintf(title, sizeof(title), "Found %s MHz", mhz);
            rfs_capture_show_listening(app, "Press the button again.");
            rfs_capture_view_set_text(
                app->capture_view,
                title,
                "Listening for the signal.",
                "Press the button again.",
                "");
        } else {
            rfs_capture_show_error(app, rfs_radio_result_text(result));
        }
        return true;
    }
    case RfsEventCaptureDecoded:
    case RfsEventCaptureRaw:
        rfs_capture_to_result(app);
        return true;
    case RfsEventCaptureError:
        rfs_capture_stop(app->capture);
        rfs_capture_show_error(app, rfs_capture_error_text(app->capture));
        return true;
    case RfsEventCaptureStop:
        /* User ends a raw recording early. */
        if(rfs_capture_finish_raw(app->capture)) rfs_capture_to_result(app);
        return true;
    case RfsEventCaptureRestart:
        app->capture_frequency = 0;
        rfs_capture_begin(app);
        return true;
    case RfsEventCaptureOptions:
        rfs_capture_stop(app->capture);
        rfs_capture_discard(app->capture);
        scene_manager_next_scene(app->scene_manager, RfsSceneCaptureOptions);
        return true;
    default:
        return false;
    }
}

void rfs_scene_capture_on_exit(void* context) {
    RfStudioApp* app = context;
    /* Release the radio; any result is kept for the result screen. */
    rfs_capture_stop(app->capture);
}
