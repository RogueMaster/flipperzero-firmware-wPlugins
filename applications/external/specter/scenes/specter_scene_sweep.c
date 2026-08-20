#include "../specter_i.h"
#include <stdio.h>

static uint8_t tick_counter; // paces the "locked" LED blink
static bool calibration_handled; // the calibration result is applied exactly once

static void specter_sweep_ok_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventReset);
}

static void specter_sweep_log_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventSweepLog);
}

static void specter_sweep_left_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventCalibrate);
}

void specter_scene_sweep_on_enter(void* context) {
    SpecterApp* app = context;

    app->reader_active = false;
    app->last_click_tick = 0;
    tick_counter = 0;
    calibration_handled = false;

    specter_apply_threshold(app);
    sweep_view_set_ok_callback(app->sweep_view, specter_sweep_ok_cb, app);
    sweep_view_set_log_callback(app->sweep_view, specter_sweep_log_cb, app);
    sweep_view_set_left_callback(app->sweep_view, specter_sweep_left_cb, app);

    field_detector_start(app->detector);
    specter_stealth_enter(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewSweep);
}

/* Calibration finished: adopt the measured floor as a Custom sensitivity, so the
 * threshold now reflects this room rather than a guess baked in at build time. */
static void specter_sweep_adopt_calibration(SpecterApp* app, const FieldStats* st) {
    char msg[12];

    app->settings.custom_threshold = st->calibration_suggest;
    app->settings.sensitivity_index = SPECTER_SENS_CUSTOM;
    specter_apply_threshold(app);

    /* Deliberately NOT written to the SD card here. This runs on the event-loop
     * thread with the radio worker still sampling, and card I/O on that path -
     * while the user is very likely mashing keys to see what the scan did - is
     * exactly the kind of stall that backs up the input queue. The flag is
     * flushed on the way out of the scene, once the worker has stopped. */
    app->settings_dirty = true;

    snprintf(
        msg,
        sizeof(msg),
        "CAL %u>%u",
        (unsigned)st->calibration_floor,
        (unsigned)st->calibration_suggest);
    sweep_view_flash(app->sweep_view, msg);
    specter_notify_saved(app);
}

bool specter_scene_sweep_on_event(void* context, SceneManagerEvent event) {
    SpecterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SpecterCustomEventReset) {
            /* Mid-scan, the obvious meaning of OK is "stop this", not "reset my
             * counters" - that is what people reach for when they want out. */
            FieldStats st;
            field_detector_get(app->detector, &st);
            if(st.calibrating) {
                field_detector_calibrate_cancel(app->detector);
                calibration_handled = true; // nothing to adopt
                sweep_view_flash(app->sweep_view, "CANCELLED");
            } else {
                field_detector_reset(app->detector);
                sweep_view_flash(app->sweep_view, "RESET");
            }
            consumed = true;
        } else if(event.event == SpecterCustomEventCalibrate) {
            calibration_handled = false;
            field_detector_calibrate_begin(app->detector, SPECTER_CALIBRATE_MS);
            consumed = true;
        } else if(event.event == SpecterCustomEventSweepLog) {
            FieldStats st;
            field_detector_get(app->detector, &st);
            if(!app->settings.logging) {
                sweep_view_flash(app->sweep_view, "LOG OFF");
            } else if(specter_log_append(
                          "SWEEP",
                          "field %u%% peak %u%% hits %lu",
                          (unsigned)st.strength,
                          (unsigned)st.peak,
                          (unsigned long)st.contacts)) {
                sweep_view_flash(app->sweep_view, "LOGGED");
                specter_notify_saved(app);
            } else {
                sweep_view_flash(app->sweep_view, "LOG FAIL");
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        tick_counter++;

        FieldStats st;
        field_detector_get(app->detector, &st);
        sweep_view_update(
            app->sweep_view,
            &st,
            specter_settings_sensitivity_label(app->settings.sensitivity_index));
        sweep_view_tick(app->sweep_view);

        if(st.calibration_ready && !calibration_handled) {
            calibration_handled = true;
            specter_sweep_adopt_calibration(app, &st);
        }

        /* Alerts stay quiet while calibrating - the whole point of that window is
         * to listen to the room without reacting to it. */
        if(!st.calibrating) {
            /* edges */
            if(st.present && !app->reader_active) specter_notify_found(app);
            if(!st.present && app->reader_active) specter_notify_gone(app);
            app->reader_active = st.present;

            /* while a reader is locked on: blink + geiger clicks scaled by strength */
            if(st.present) {
                if(app->settings.led && (tick_counter % 3u == 0u)) specter_notify_present_led(app);

                if(app->settings.sound) {
                    uint32_t interval = 360u - 3u * st.strength;
                    if(interval < 70u) interval = 70u;
                    if(interval > 360u) interval = 360u;
                    uint32_t now = furi_get_tick();
                    if((uint32_t)(now - app->last_click_tick) >= interval) {
                        specter_notify_click(app);
                        app->last_click_tick = now;
                    }
                }
            }
        }
        consumed = true;
    }
    return consumed;
}

void specter_scene_sweep_on_exit(void* context) {
    SpecterApp* app = context;
    field_detector_stop(app->detector);
    if(app->settings_dirty) {
        /* Worker is stopped, so the card is not competing with the radio. */
        specter_settings_save(&app->settings);
        app->settings_dirty = false;
    }
    specter_stealth_exit(app);
}
