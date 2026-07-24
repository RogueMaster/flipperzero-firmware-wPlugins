#include "../specter_i.h"
#include <stdio.h>

/* Unattended monitor. Unlike Sweep (you hunting) or Survey (a bounded verdict),
 * Watch stands guard indefinitely: arm it, set the Flipper down, walk away. It
 * counts contacts, remembers when the last one was, and - the whole point -
 * wakes the screen and sounds off the instant a reader appears. It deliberately
 * ignores stealth: a silent, dark guard that never tells you it saw something
 * would be worse than useless. */

#define WATCH_LOG_MIN_INTERVAL_MS 3000u // don't spam the log if a reader flaps
#define WATCH_LED_EVERY_TICKS     3u

static uint32_t watch_start_tick;
static uint32_t watch_first_ms;
static uint32_t watch_last_ms;
static uint32_t watch_last_contacts;
static uint32_t watch_last_click_tick;
static uint32_t watch_last_log_tick;
static uint8_t watch_tick_counter;
static bool watch_present_prev;

static void specter_watch_reset_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventWatchReset);
}

static void specter_watch_arm(SpecterApp* app) {
    watch_first_ms = WATCH_NO_TIME;
    watch_last_ms = WATCH_NO_TIME;
    watch_last_contacts = 0;
    watch_last_click_tick = 0;
    watch_last_log_tick = 0;
    watch_tick_counter = 0;
    watch_present_prev = false;

    specter_apply_threshold(app);
    field_detector_stop(app->detector); // always start from clean counters
    field_detector_start(app->detector);
    watch_start_tick = furi_get_tick(); // clock zero = the moment we armed
}

void specter_scene_watch_on_enter(void* context) {
    SpecterApp* app = context;
    watch_view_set_reset_callback(app->watch_view, specter_watch_reset_cb, app);
    specter_watch_arm(app);
    /* No stealth here on purpose - Watch must be free to light up on a hit. */
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewWatch);
}

bool specter_scene_watch_on_event(void* context, SceneManagerEvent event) {
    SpecterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SpecterCustomEventWatchReset) {
            specter_watch_arm(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        watch_tick_counter++;

        FieldStats st;
        field_detector_get(app->detector, &st);
        uint32_t now = furi_get_tick();
        uint32_t watching_ms = now - watch_start_tick;

        /* A brand-new contact appeared since we last looked. */
        if(st.contacts > watch_last_contacts) {
            watch_last_contacts = st.contacts;
            watch_last_ms = watching_ms;
            if(watch_first_ms == WATCH_NO_TIME) watch_first_ms = watching_ms;

            specter_notify_wake(app); // pull the backlight on so a glance catches it

            if(app->settings.logging &&
               (uint32_t)(now - watch_last_log_tick) >= WATCH_LOG_MIN_INTERVAL_MS) {
                specter_log_append(
                    "WATCH",
                    "contact %lu at %lus field %u%% peak %u%%",
                    (unsigned long)st.contacts,
                    (unsigned long)(watching_ms / 1000u),
                    (unsigned)st.strength,
                    (unsigned)st.peak);
                watch_last_log_tick = now;
            }
        }

        /* Alert edges + ongoing alarm, mirroring the sweep screen. */
        if(!st.error) {
            if(st.present && !watch_present_prev) specter_notify_found(app);
            if(!st.present && watch_present_prev) specter_notify_gone(app);
            watch_present_prev = st.present;

            if(st.present) {
                if(app->settings.led && (watch_tick_counter % WATCH_LED_EVERY_TICKS == 0u))
                    specter_notify_present_led(app);
                if(app->settings.sound) {
                    uint32_t interval = 360u - 3u * st.strength;
                    if(interval < 70u) interval = 70u;
                    if(interval > 360u) interval = 360u;
                    if((uint32_t)(now - watch_last_click_tick) >= interval) {
                        specter_notify_click(app);
                        watch_last_click_tick = now;
                    }
                }
            }
        }

        watch_view_update(app->watch_view, &st, watching_ms, watch_first_ms, watch_last_ms);
        watch_view_tick(app->watch_view);
        consumed = true;
    }
    return consumed;
}

void specter_scene_watch_on_exit(void* context) {
    SpecterApp* app = context;
    field_detector_stop(app->detector);
    specter_stealth_exit(app); // harmless if never engaged; keeps the invariant
}
