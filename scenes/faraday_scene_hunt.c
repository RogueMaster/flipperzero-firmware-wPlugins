#include "../faraday_i.h"
#include <stdio.h>

/* Below this margin over the noise floor there is nothing to hear. */
#define FDY_HUNT_FLOOR_DB 4

/* Click pacing: fastest when the leak is blazing, slowest when barely warm. */
#define FDY_CLICK_SLOW_MS 420
#define FDY_CLICK_FAST_MS 70
#define FDY_CLICK_PER_DB 11

static char hunt_band_label[16]; // static: HuntData holds the pointer

static void faraday_hunt_ok_cb(void* context) {
    FaradayApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FaradayCustomEventOk);
}

void faraday_scene_hunt_on_enter(void* context) {
    FaradayApp* app = context;

    uint8_t bi = app->settings.band_index;
    if(bi >= FDY_BAND_COUNT) bi = 1;
    snprintf(hunt_band_label, sizeof(hunt_band_label), "%s MHz", fdy_bands[bi].label);

    app->last_click_tick = 0;

    fdy_subghz_set_freq(app->subghz, fdy_bands[bi].frequency);
    hunt_view_set_ok_callback(app->hunt_view, faraday_hunt_ok_cb, app);

    fdy_subghz_start(app->subghz);
    fdy_subghz_reset_peak(app->subghz);

    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewHunt);
}

bool faraday_scene_hunt_on_event(void* context, SceneManagerEvent event) {
    FaradayApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FaradayCustomEventOk) {
            fdy_subghz_reset_peak(app->subghz); // re-sweep this spot cleanly
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        FdySubGhzSnapshot sn;
        fdy_subghz_get(app->subghz, &sn);

        HuntData d;
        memset(&d, 0, sizeof(d));
        d.band = hunt_band_label;
        d.rssi = sn.rssi;
        d.peak = sn.peak;
        d.floor = sn.floor;
        d.level = sn.level;
        d.peak_norm = sn.peak_norm;
        d.history = sn.history;
        d.history_head = sn.history_head;

        hunt_view_update(app->hunt_view, &d);
        hunt_view_tick(app->hunt_view);

        /* Geiger clicks off the same margin the screen is showing, so what you
         * hear and what you see can never disagree. */
        int16_t margin = hunt_view_margin(&d);
        if(app->settings.sound && margin >= FDY_HUNT_FLOOR_DB) {
            uint32_t interval = FDY_CLICK_SLOW_MS - (uint32_t)margin * FDY_CLICK_PER_DB;
            if(interval < FDY_CLICK_FAST_MS) interval = FDY_CLICK_FAST_MS;
            if(interval > FDY_CLICK_SLOW_MS) interval = FDY_CLICK_SLOW_MS;
            uint32_t now = furi_get_tick();
            if((uint32_t)(now - app->last_click_tick) >= interval) {
                faraday_notify_click(app);
                app->last_click_tick = now;
            }
        }
        consumed = true;
    }
    return consumed;
}

void faraday_scene_hunt_on_exit(void* context) {
    FaradayApp* app = context;
    fdy_subghz_stop(app->subghz);
}
