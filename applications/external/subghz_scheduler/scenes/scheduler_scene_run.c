#include "src/scheduler_app_i.h"
#include "src/scheduler_run.h"

#include <lib/subghz/devices/devices.h>

void scheduler_scene_run_on_enter(void* context) {
    SchedulerApp* app = context;
    furi_hal_power_suppress_charge_enter();
    scheduler_time_reset(app->scheduler);

    scheduler_run_view_set_static_fields(app->run_view, app->scheduler);
    scheduler_run_view_update_countdown(app->run_view, app->scheduler);

    subghz_devices_init();
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewRunSchedule);
}

void scheduler_scene_run_on_exit(void* context) {
    SchedulerApp* app = context;

    if(app->thread != NULL) {
        furi_thread_join(app->thread);
        app->thread = NULL;
    }

    scheduler_time_reset(app->scheduler);
    subghz_devices_deinit();
    furi_hal_power_suppress_charge_exit();
}

bool scheduler_scene_run_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    const uint8_t tick_counter = scheduler_run_view_get_tick_counter(app->run_view);
    bool consumed = false;

    const bool update = (!app->is_transmitting || scheduler_get_timing_mode(app->scheduler)) &&
                        !(tick_counter % 2);

    if(event.type == SceneManagerEventTypeTick) {
        if(update) {
            scheduler_run_view_update_countdown(app->run_view, app->scheduler);

            const bool trig = scheduler_time_to_trigger(app->scheduler);
            if(trig) {
                scheduler_start_tx(app);
            } else {
                notification_message(app->notifications, &sequence_blink_red_10);
            }
        }

        scheduler_run_view_inc_tick_counter(app->run_view);
        consumed = true;
    }

    return consumed;
}
