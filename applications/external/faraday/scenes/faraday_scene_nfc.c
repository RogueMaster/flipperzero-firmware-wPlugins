#include "../faraday_i.h"

/* The baseline needs a reader actually energising the Flipper's antenna. Below
 * this duty-cycle there is no field worth measuring against. */
#define FDY_NFC_MIN_FIELD 10

/* meter_view.c mirrors these three so a radio-agnostic view need not include
 * the NFC helper's header. This is the check that keeps the copy honest: if
 * FdyNfcCapture is ever reordered, the build stops here rather than the
 * shielded countdown quietly reporting the wrong stage. */
_Static_assert((int)FdyNfcCaptureLive == FDY_NFC_LIVE, "FDY_NFC_LIVE out of step");
_Static_assert((int)FdyNfcCaptureArming == FDY_NFC_ARMING, "FDY_NFC_ARMING out of step");
_Static_assert((int)FdyNfcCaptureMeasuring == FDY_NFC_MEASURING, "FDY_NFC_MEASURING out of step");

/* The meter view is shared by both radios and keeps the last frame it was
 * given, so switching to it before pushing anything shows the PREVIOUS test's
 * verdict for up to one tick - a stale grade, on entry, that looks exactly
 * like a real one. Push an empty frame first. */
static void faraday_meter_prime(FaradayApp* app, bool is_nfc, const char* band) {
    MeterData d;
    memset(&d, 0, sizeof(d));
    d.is_nfc = is_nfc;
    d.band = band;
    d.phase = FdyPhaseBaseline;
    d.ceiling = (uint8_t)FdyRatingCount;
    meter_view_update(app->meter_view, &d);
}

static void faraday_nfc_ok_cb(void* context) {
    FaradayApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FaradayCustomEventOk);
}

void faraday_scene_nfc_on_enter(void* context) {
    FaradayApp* app = context;

    fdy_test_reset(&app->test);
    meter_view_set_ok_callback(app->meter_view, faraday_nfc_ok_cb, app);
    meter_view_reset_intro(app->meter_view);
    meter_view_flash_notice(app->meter_view, NULL); // no stale refusal on entry

    fdy_nfc_start(app->nfc);
    fdy_nfc_reset_peak(app->nfc);

    faraday_meter_prime(app, true, "13.56 MHz");
    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewMeter);
}

bool faraday_scene_nfc_on_event(void* context, SceneManagerEvent event) {
    FaradayApp* app = context;
    FdyTest* t = &app->test;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FaradayCustomEventOk) {
            FdyNfcSnapshot sn;
            fdy_nfc_get(app->nfc, &sn);
            if(sn.error) return true; // nothing to do while the chip is held elsewhere

            if(t->phase == FdyPhaseBaseline) {
                if(sn.peak < FDY_NFC_MIN_FIELD) {
                    /* Visible as well as audible - see the Sub-GHz scene. */
                    meter_view_flash_notice(app->meter_view, "No reader field yet");
                    faraday_notify_reject(app);
                } else {
                    t->base_value = (int16_t)sn.peak;
                    t->base_norm = sn.peak;
                    t->have_base = true;
                    t->phase = FdyPhaseShield;
                    /* Not a bare peak reset: the Flipper itself goes in the
                     * pouch here, so the user cannot watch the screen or press
                     * a key during the measurement. The worker runs it on a
                     * timer and freezes the result. */
                    fdy_nfc_begin_shielded(app->nfc);
                    faraday_notify_lock(app);
                }
            } else if(t->phase == FdyPhaseShield) {
                if(sn.capture != (uint8_t)FdyNfcCaptureFrozen) {
                    /* Reading the peak now would report whatever the Flipper
                     * could see while it was still in the user's hand. */
                    meter_view_flash_notice(app->meter_view, "Still measuring - wait");
                    faraday_notify_reject(app);
                    return true;
                }
                t->shield_value = (int16_t)sn.peak;
                t->shield_norm = sn.peak;
                t->have_shield = true;

                /* Percentage of the interrogation field the pouch kept out. */
                int32_t blocked = 0;
                if(t->base_value > 0) {
                    blocked = ((int32_t)(t->base_value - t->shield_value) * 100) / t->base_value;
                }
                if(blocked < 0) blocked = 0;
                if(blocked > 100) blocked = 100;

                t->atten = (int16_t)blocked;
                t->atten_floored = false; // a percentage is already capped at 100
                t->rating = (uint8_t)fdy_grade_pct((uint8_t)blocked);
                t->phase = FdyPhaseVerdict;

                if(!faraday_log_result(app, true, 0)) {
                    /* Do not let a finished test look saved when it is not. */
                    meter_view_flash_notice(app->meter_view, "Not saved - check SD card");
                }

                faraday_notify_verdict(app, t->rating);
            } else { // verdict -> run it again
                fdy_test_reset(t);
                fdy_nfc_reset_peak(app->nfc);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        FdyNfcSnapshot sn;
        fdy_nfc_get(app->nfc, &sn);

        MeterData d;
        memset(&d, 0, sizeof(d));
        d.is_nfc = true;
        d.band = "13.56 MHz";
        d.phase = sn.error ? FdyPhaseError : t->phase;
        d.capture = sn.capture;
        d.capture_seconds = sn.seconds;
        d.err1 = "NFC busy";
        d.err2 = "Close other NFC apps, retry.";
        d.level = sn.strength;
        d.peak = sn.peak;
        d.live_value = (int16_t)sn.strength;
        /* The SAME test the lock uses. The cue used to be sn.present (duty
         * over 4%) while locking needed peak >= 10%, so the screen said the
         * signal was fine and OK then refused it - the exact shape of trust
         * bug that makes a working button look broken. */
        d.signal_ok = sn.peak >= FDY_NFC_MIN_FIELD;
        d.margin = (int16_t)sn.peak;
        /* NFC is scored as a percentage of the field blocked, so a peak of
         * N%% caps the test at whatever N%% blocked would grade. */
        d.ceiling = sn.present ? (uint8_t)fdy_grade_pct(sn.peak) : (uint8_t)FdyRatingCount;
        d.have_base = t->have_base;
        d.have_shield = t->have_shield;
        d.base_value = t->base_value;
        d.shield_value = t->shield_value;
        d.base_norm = t->base_norm;
        d.shield_norm = t->shield_norm;
        d.shield_floored = t->shield_floored;
        d.atten = t->atten;
        d.atten_floored = t->atten_floored;
        d.rating = t->rating;
        d.history = sn.history;
        d.history_head = sn.history_head;

        meter_view_update(app->meter_view, &d);
        meter_view_tick(app->meter_view);
        consumed = true;
    }
    return consumed;
}

void faraday_scene_nfc_on_exit(void* context) {
    FaradayApp* app = context;
    fdy_nfc_stop(app->nfc);
}
