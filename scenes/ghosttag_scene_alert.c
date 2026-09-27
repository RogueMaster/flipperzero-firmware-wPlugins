#include "../ghosttag_i.h"

static void ghosttag_scene_alert_ok_cb(void* context) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventOpenDetail);
}

void ghosttag_scene_alert_on_enter(void* context) {
    GhostTagApp* app = context;
    alert_view_set_ok_callback(app->alert_view, ghosttag_scene_alert_ok_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewAlert);
}

bool ghosttag_scene_alert_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        /* A second tracker tripped while this alert is still up. Re-point THIS
         * alert at it rather than stacking another scene on top, so the user is
         * never left pressing Back through a pile of alarms. */
        if(tracker_db_take_pending_alert(app->db, &app->alert_record)) {
            if(app->source == GhostTagSourceEsp32 && session_log_is_open(app->log)) {
                session_log_follower(app->log, &app->alert_record);
            }
            alert_view_set_record(
                app->alert_view, &app->alert_record, app->source == GhostTagSourceDemo);
            ghosttag_notify_alert(app);
        }
        alert_view_tick(app->alert_view);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GhostTagCustomEventOpenDetail:
            app->detail_record = app->alert_record;
            device_detail_view_set_record(app->device_detail_view, &app->detail_record);
            device_detail_view_set_demo(
                app->device_detail_view, app->source == GhostTagSourceDemo);
            /* Pop this alert BEFORE pushing the detail screen. Pushing on top
             * of it would leave the alert on the stack, so Back out of the
             * details re-opened an alarm the user had already dealt with -
             * strobing banner and all. */
            scene_manager_previous_scene(app->scene_manager);
            scene_manager_next_scene(app->scene_manager, GhostTagSceneDetail);
            return true;

        default:
            return false;
        }
    }
    return false;
}

void ghosttag_scene_alert_on_exit(void* context) {
    GhostTagApp* app = context;
    alert_view_set_ok_callback(app->alert_view, NULL, NULL);
}
