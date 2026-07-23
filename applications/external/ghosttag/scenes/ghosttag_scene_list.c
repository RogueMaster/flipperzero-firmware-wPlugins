#include "../ghosttag_i.h"

static void ghosttag_scene_list_ok_cb(void* context) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventOpenDetail);
}

static void ghosttag_scene_list_refresh(GhostTagApp* app) {
    TrackerRecord snap[TRACKER_DB_MAX];
    size_t n = tracker_db_snapshot(app->db, snap, TRACKER_DB_MAX);
    device_list_view_set_records(app->device_list_view, snap, n);
}

void ghosttag_scene_list_on_enter(void* context) {
    GhostTagApp* app = context;
    device_list_view_set_ok_callback(app->device_list_view, ghosttag_scene_list_ok_cb, app);
    ghosttag_scene_list_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewDeviceList);
}

bool ghosttag_scene_list_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        // Keep the list live while a hunt is running.
        if(uart_link_is_running(app->uart)) ghosttag_scene_list_refresh(app);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GhostTagCustomEventOpenDetail:
            if(device_list_view_get_selected(app->device_list_view, &app->selected_record)) {
                device_detail_view_set_record(app->device_detail_view, &app->selected_record);
                scene_manager_next_scene(app->scene_manager, GhostTagSceneDetail);
            }
            consumed = true;
            break;
        case GhostTagCustomEventNewFollower:
            if(tracker_db_take_pending_alert(app->db, &app->selected_record)) {
                alert_view_set_record(app->alert_view, &app->selected_record);
                ghosttag_notify_alert(app);
                scene_manager_next_scene(app->scene_manager, GhostTagSceneAlert);
            }
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void ghosttag_scene_list_on_exit(void* context) {
    UNUSED(context);
}
