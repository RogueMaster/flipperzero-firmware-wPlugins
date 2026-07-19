#include "../cerberus_i.h"

// Bridges Watch-screen gestures: short OK = recalibrate, long OK = arm/silent.
static void cerberus_scene_monitor_view_callback(CerberusMonitorEvent event, void* context) {
    Cerberus* app = context;
    switch(event) {
    case CerberusMonitorEventResetFloor:
        cerberus_subghz_reset_floor(app->worker);
        notification_message(app->notifications, &sequence_blink_blue_100);
        break;
    case CerberusMonitorEventToggleArm:
        app->armed = !app->armed;
        cerberus_monitor_view_set_status(app->monitor, app->armed, app->alert_total);
        notification_message(
            app->notifications,
            app->armed ? &sequence_blink_green_100 : &sequence_blink_yellow_100);
        break;
    default:
        break;
    }
}

void cerberus_scene_monitor_on_enter(void* context) {
    Cerberus* app = context;

    cerberus_monitor_view_set_callback(app->monitor, cerberus_scene_monitor_view_callback, app);
    cerberus_monitor_view_set_status(app->monitor, app->armed, app->alert_total);

    if(app->config.keep_screen_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    cerberus_apply_config(app);
    cerberus_subghz_start(app->worker);

    view_dispatcher_switch_to_view(app->view_dispatcher, CerberusViewMonitor);
}

bool cerberus_scene_monitor_on_event(void* context, SceneManagerEvent event) {
    Cerberus* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        CerberusSnapshot snap;
        cerberus_subghz_get_snapshot(app->worker, &snap);
        cerberus_monitor_view_update(app->monitor, &snap);
        cerberus_monitor_view_set_status(app->monitor, app->armed, app->alert_total);
        consumed = true;
    }

    return consumed;
}

void cerberus_scene_monitor_on_exit(void* context) {
    Cerberus* app = context;
    cerberus_subghz_stop(app->worker);
    if(app->config.keep_screen_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    }
    cerberus_monitor_view_set_callback(app->monitor, NULL, NULL);
}
