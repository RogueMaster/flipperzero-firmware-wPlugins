#include "../specter_i.h"

static uint8_t tick_counter; // paces the "locked" LED blink

static void specter_sweep_ok_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventReset);
}

void specter_scene_sweep_on_enter(void* context) {
    SpecterApp* app = context;

    app->reader_active = false;
    app->last_click_tick = 0;
    tick_counter = 0;

    field_detector_set_threshold(app->detector, specter_settings_threshold(&app->settings));
    sweep_view_set_ok_callback(app->sweep_view, specter_sweep_ok_cb, app);

    field_detector_start(app->detector);
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewSweep);
}

bool specter_scene_sweep_on_event(void* context, SceneManagerEvent event) {
    SpecterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SpecterCustomEventReset) {
            field_detector_reset(app->detector);
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
        consumed = true;
    }
    return consumed;
}

void specter_scene_sweep_on_exit(void* context) {
    SpecterApp* app = context;
    field_detector_stop(app->detector);
}
