#include "../nyx_i.h"

static uint8_t tick_counter; // paces the "locked" LED blink

static void nyx_sweep_ok_cb(void* context) {
    NyxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NyxCustomEventReset);
}

static void nyx_sweep_long_ok_cb(void* context) {
    NyxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NyxCustomEventRenull);
}

void nyx_scene_sweep_on_enter(void* context) {
    NyxApp* app = context;

    app->was_present = false;
    app->last_click_tick = 0;
    tick_counter = 0;

    ir_sense_set_mode(app->sense, (IrSenseMode)app->settings.mode_index);
    ir_sense_set_sensitivity(app->sense, app->settings.sensitivity_index);
    ir_sense_set_probe_pin(app->sense, app->settings.probe_pin_index);

    sweep_view_set_ok_callback(app->sweep_view, nyx_sweep_ok_cb, app);
    sweep_view_set_long_ok_callback(app->sweep_view, nyx_sweep_long_ok_cb, app);

    ir_sense_start(app->sense);
    view_dispatcher_switch_to_view(app->view_dispatcher, NyxViewSweep);
}

bool nyx_scene_sweep_on_event(void* context, SceneManagerEvent event) {
    NyxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NyxCustomEventReset) {
            ir_sense_reset(app->sense);
            consumed = true;
        } else if(event.event == NyxCustomEventRenull) {
            ir_sense_renull(app->sense);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        tick_counter++;

        IrStats st;
        ir_sense_get(app->sense, &st);
        sweep_view_update(app->sweep_view, &st);
        sweep_view_tick(app->sweep_view);

        /* edges */
        if(st.present && !app->was_present) nyx_notify_found(app);
        if(!st.present && app->was_present) nyx_notify_gone(app);
        app->was_present = st.present;

        /* while an emitter is locked on: blink + geiger clicks that speed up as
         * the reading climbs, so you can hunt with the screen at your side */
        if(st.present) {
            if(app->settings.led && (tick_counter % 3u == 0u)) nyx_notify_present_led(app);

            if(app->settings.sound) {
                uint32_t interval = 360u - 3u * st.level;
                if(interval < 70u) interval = 70u;
                if(interval > 360u) interval = 360u;
                uint32_t now = furi_get_tick();
                if((uint32_t)(now - app->last_click_tick) >= interval) {
                    nyx_notify_click(app);
                    app->last_click_tick = now;
                }
            }
        }
        consumed = true;
    }
    return consumed;
}

void nyx_scene_sweep_on_exit(void* context) {
    NyxApp* app = context;
    ir_sense_stop(app->sense);
}
