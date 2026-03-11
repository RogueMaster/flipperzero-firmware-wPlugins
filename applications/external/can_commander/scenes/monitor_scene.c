#include "../can_commander.h"

void cancommander_scene_monitor_on_enter(void* context) {
    App* app = context;

    app->monitor_scene_active = true;
    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
        app->monitor_update_pending = true;
        app->monitor_last_update_ms = furi_get_tick();
        furi_mutex_release(app->mutex);
    }
    app_refresh_live_view(app);
}

bool cancommander_scene_monitor_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == AppCustomEventMonitorUpdated) {
        if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
            app->monitor_update_pending = true;
            furi_mutex_release(app->mutex);
        }
        return true;
    }

    if(event.type == SceneManagerEventTypeTick) {
        bool is_reverse_dashboard = false;
        bool pending = false;
        if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
            is_reverse_dashboard = (app->dashboard_mode == AppDashboardReverse);
            pending = app->monitor_update_pending;
            app->monitor_update_pending = false;
            if(pending || is_reverse_dashboard) {
                app->monitor_last_update_ms = furi_get_tick();
            }
            furi_mutex_release(app->mutex);
        }

        if(pending || is_reverse_dashboard) {
            app_refresh_live_view(app);
            return true;
        }
    }

    return false;
}

void cancommander_scene_monitor_on_exit(void* context) {
    App* app = context;
    app->monitor_scene_active = false;
    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
        app->monitor_update_pending = false;
        furi_mutex_release(app->mutex);
    }
}
