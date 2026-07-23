#include "../faraday_i.h"

/* The baseline needs a reader actually energising the Flipper's antenna. Below
 * this duty-cycle there is no field worth measuring against. */
#define FDY_NFC_MIN_FIELD 10

static void faraday_nfc_ok_cb(void* context) {
    FaradayApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FaradayCustomEventOk);
}

void faraday_scene_nfc_on_enter(void* context) {
    FaradayApp* app = context;

    fdy_test_reset(&app->test);
    meter_view_set_ok_callback(app->meter_view, faraday_nfc_ok_cb, app);

    fdy_nfc_start(app->nfc);
    fdy_nfc_reset_peak(app->nfc);

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
                    faraday_notify_reject(app); // no reader field to measure
                } else {
                    t->base_value = (int16_t)sn.peak;
                    t->base_norm = sn.peak;
                    t->have_base = true;
                    t->phase = FdyPhaseShield;
                    fdy_nfc_reset_peak(app->nfc);
                    faraday_notify_lock(app);
                }
            } else if(t->phase == FdyPhaseShield) {
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

                faraday_log_result(app, true, 0);

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
        d.err1 = "NFC busy";
        d.err2 = "Close other NFC apps, retry.";
        d.level = sn.strength;
        d.peak = sn.peak;
        d.live_value = (int16_t)sn.strength;
        d.signal_ok = sn.present;
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
