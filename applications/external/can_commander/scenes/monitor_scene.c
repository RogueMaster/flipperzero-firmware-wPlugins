#include "../can_commander.h"

void cancommander_scene_monitor_on_enter(void* context) {
    App* app = context;

    app->monitor_scene_active = true;
    app_refresh_live_view(app);
}

bool cancommander_scene_monitor_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == AppCustomEventMonitorUpdated) {
        app_refresh_live_view(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeTick) {
        bool is_reverse_dashboard = false;
        if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
            is_reverse_dashboard = (app->dashboard_mode == AppDashboardReverse);
            furi_mutex_release(app->mutex);
        }

        if(is_reverse_dashboard) {
            app_refresh_live_view(app);
            return true;
        }
    }

    return false;
}

void cancommander_scene_monitor_on_exit(void* context) {
    App* app = context;
    app->monitor_scene_active = false;
}
