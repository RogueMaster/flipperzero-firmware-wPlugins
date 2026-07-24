#include "../specter_i.h"
#include <stdio.h>

/* Hold the Flipper still against the emitter you found with a sweep. This screen
 * does not chase proximity - it watches the carrier's rhythm and says what kind
 * of thing is making it. */

static void specter_fingerprint_save_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventFingerprintSave);
}

static void specter_fingerprint_reset_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventFingerprintReset);
}

void specter_scene_fingerprint_on_enter(void* context) {
    SpecterApp* app = context;

    specter_apply_threshold(app);
    fingerprint_view_set_save_callback(app->fingerprint_view, specter_fingerprint_save_cb, app);
    fingerprint_view_set_reset_callback(app->fingerprint_view, specter_fingerprint_reset_cb, app);

    field_detector_start(app->detector);
    specter_stealth_enter(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewFingerprint);
}

static void specter_fingerprint_save(SpecterApp* app) {
    FieldStats st;
    field_detector_get(app->detector, &st);
    EmitterVerdict v = emitter_classify(&st.cadence);

    if(!app->settings.logging) {
        fingerprint_view_flash(app->fingerprint_view, "LOG OFF");
        return;
    }

    /* Nothing worth writing down yet - say so rather than logging an empty
     * finding that looks like evidence later. */
    if(v.klass == EmitterClassNoField || v.klass == EmitterClassUnknown) {
        fingerprint_view_flash(app->fingerprint_view, "NO DATA");
        return;
    }

    bool ok;
    if(v.klass == EmitterClassContinuous) {
        ok = specter_log_append(
            "READER",
            "%s duty %u%% conf %u%%",
            emitter_class_name(v.klass),
            (unsigned)st.cadence.duty,
            (unsigned)v.confidence);
    } else {
        ok = specter_log_append(
            "READER",
            "%s period %s%ums burst %ums duty %u%% conf %u%%",
            emitter_class_name(v.klass),
            v.timing_reliable ? "" : "~",
            (unsigned)st.cadence.period_ms,
            (unsigned)st.cadence.burst_ms,
            (unsigned)st.cadence.duty,
            (unsigned)v.confidence);
    }

    if(ok) {
        fingerprint_view_flash(app->fingerprint_view, "LOGGED");
        specter_notify_saved(app);
    } else {
        fingerprint_view_flash(app->fingerprint_view, "LOG FAIL");
    }
}

bool specter_scene_fingerprint_on_event(void* context, SceneManagerEvent event) {
    SpecterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SpecterCustomEventFingerprintSave) {
            specter_fingerprint_save(app);
            consumed = true;
        } else if(event.event == SpecterCustomEventFingerprintReset) {
            field_detector_reset(app->detector);
            fingerprint_view_flash(app->fingerprint_view, "RESET");
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        FieldStats st;
        field_detector_get(app->detector, &st);
        fingerprint_view_update(app->fingerprint_view, &st);
        fingerprint_view_tick(app->fingerprint_view);
        consumed = true;
    }
    return consumed;
}

void specter_scene_fingerprint_on_exit(void* context) {
    SpecterApp* app = context;
    field_detector_stop(app->detector);
    specter_stealth_exit(app);
}
