#include "../faraday_i.h"
#include <stdio.h>

/* A capture only counts if the carrier actually rose out of the noise. Below
 * this margin we assume the fob never transmitted and refuse to lock.
 *
 * MEASURED, not guessed. This was 8 dB, which the peak-hold cleared on bare
 * ambient noise: on a quiet 433.92 MHz channel with nothing transmitting, the
 * peak reached +13 dB above the tracked floor within 5 seconds of opening the
 * screen and +14 dB by 90 seconds. At 8 dB the refusal that is supposed to
 * catch "you never pressed the fob" therefore passed almost immediately, and
 * the app would happily grade a baseline made of noise.
 *
 * A real fob at arm's length sits 30-60 dB over the floor, so 20 dB separates
 * the two cleanly: comfortably above the noise ceiling measured here, and far
 * below any genuine carrier. The peak is reset on entering each phase, so the
 * figure the user locks is always from the current phase, and the live "PK"
 * readout on screen shows them exactly what this is being compared against. */
#define FDY_SIGNAL_MARGIN_DB 20

/* If the shielded peak lands this close to the noise floor, the signal was
 * buried: the true attenuation is at least what we measured, possibly more. */
#define FDY_FLOOR_MARGIN_DB 3

static char band_label[16]; // static: MeterData holds the pointer, not a copy

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

static void faraday_subghz_ok_cb(void* context) {
    FaradayApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FaradayCustomEventOk);
}

void faraday_scene_subghz_on_enter(void* context) {
    FaradayApp* app = context;

    fdy_test_reset(&app->test);

    uint8_t bi = app->settings.band_index;
    if(bi >= FDY_BAND_COUNT) bi = 1;
    snprintf(band_label, sizeof(band_label), "%s MHz", fdy_bands[bi].label);

    fdy_subghz_set_freq(app->subghz, fdy_bands[bi].frequency);
    meter_view_set_ok_callback(app->meter_view, faraday_subghz_ok_cb, app);
    meter_view_reset_intro(app->meter_view);
    meter_view_flash_notice(app->meter_view, NULL); // no stale refusal on entry

    fdy_subghz_start(app->subghz);
    fdy_subghz_reset_peak(app->subghz);

    faraday_meter_prime(app, false, band_label);
    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewMeter);
}

bool faraday_scene_subghz_on_event(void* context, SceneManagerEvent event) {
    FaradayApp* app = context;
    FdyTest* t = &app->test;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FaradayCustomEventOk) {
            FdySubGhzSnapshot sn;
            fdy_subghz_get(app->subghz, &sn);
            bool has_signal = (sn.peak - sn.floor) >= FDY_SIGNAL_MARGIN_DB;

            if(t->phase == FdyPhaseBaseline) {
                if(!has_signal) {
                    /* Say so on screen as well as in the speaker: a beep with
                     * no visible change is why a refusal reads as a dead
                     * button. */
                    meter_view_flash_notice(app->meter_view, "Press the fob first");
                    faraday_notify_reject(app);
                } else {
                    t->base_value = sn.peak;
                    t->base_norm = sn.peak_norm;
                    t->have_base = true;
                    t->phase = FdyPhaseShield;
                    fdy_subghz_reset_peak(app->subghz);
                    faraday_notify_lock(app);
                }
            } else if(t->phase == FdyPhaseShield) {
                /* A shielded capture with no detectable carrier is a valid
                 * (and excellent) result - we clamp it to the noise floor and
                 * report the attenuation as a lower bound. */
                t->shield_value = has_signal ? sn.peak : sn.floor;
                t->shield_norm = has_signal ? sn.peak_norm : fdy_subghz_normalize(sn.floor);
                t->shield_floored = (t->shield_value - sn.floor) <= FDY_FLOOR_MARGIN_DB;
                t->have_shield = true;

                t->atten = (int16_t)(t->base_value - t->shield_value);
                t->atten_floored = t->shield_floored;
                t->rating = (uint8_t)fdy_grade_db(t->atten);
                t->phase = FdyPhaseVerdict;

                uint8_t bi = app->settings.band_index;
                if(bi >= FDY_BAND_COUNT) bi = 1;
                if(!faraday_log_result(app, false, fdy_bands[bi].frequency)) {
                    /* Do not let a finished test look saved when it is not. */
                    meter_view_flash_notice(app->meter_view, "Not saved - check SD card");
                }

                faraday_notify_verdict(app, t->rating);
            } else { // verdict -> run it again
                fdy_test_reset(t);
                fdy_subghz_reset_peak(app->subghz);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        FdySubGhzSnapshot sn;
        fdy_subghz_get(app->subghz, &sn);

        MeterData d;
        memset(&d, 0, sizeof(d));
        d.is_nfc = false;
        d.band = band_label;
        d.phase = t->phase;
        d.level = sn.level;
        d.peak = sn.peak_norm;
        d.live_value = sn.rssi;
        d.signal_ok = (sn.peak - sn.floor) >= FDY_SIGNAL_MARGIN_DB;
        d.margin = (int16_t)(sn.peak - sn.floor);
        /* What the floor allows, not what the pouch achieves. */
        d.ceiling = d.signal_ok ? (uint8_t)fdy_grade_db(d.margin) : (uint8_t)FdyRatingCount;
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

void faraday_scene_subghz_on_exit(void* context) {
    FaradayApp* app = context;
    fdy_subghz_stop(app->subghz);
}
