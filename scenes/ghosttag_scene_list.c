#include "../ghosttag_i.h"

static void ghosttag_scene_list_ok_cb(void* context) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventOpenDetail);
}

static DeviceListState ghosttag_list_state(GhostTagApp* app) {
    if(app->source == GhostTagSourceDemo) return DeviceListStateDemo;
    if(app->source != GhostTagSourceEsp32) return DeviceListStateIdle;
    return app->esp_connected ? DeviceListStateLive : DeviceListStateWaiting;
}

static void ghosttag_scene_list_refresh(GhostTagApp* app) {
    size_t n = tracker_db_snapshot(app->db, app->scratch, TRACKER_DB_MAX);
    device_list_view_set_records(app->device_list_view, app->scratch, n);
    device_list_view_set_state(app->device_list_view, ghosttag_list_state(app));
}

void ghosttag_scene_list_on_enter(void* context) {
    GhostTagApp* app = context;
    device_list_view_set_ok_callback(app->device_list_view, ghosttag_scene_list_ok_cb, app);
    ghosttag_scene_list_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewDeviceList);
}

bool ghosttag_scene_list_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        if(ghosttag_poll_alert(app)) return true;
        if(ghosttag_is_hunting(app)) {
            /* The link can drop while the list is open, so the LIVE / NO BOARD
             * badge has to be re-evaluated, not set once on entry. */
            ghosttag_update_link(app);
            ghosttag_scene_list_refresh(app);
        }
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GhostTagCustomEventOpenDetail:
            if(device_list_view_get_selected(app->device_list_view, &app->detail_record)) {
                scene_manager_next_scene(app->scene_manager, GhostTagSceneDetail);
            } else {
                /* Nothing to open. A press that changes nothing on screen is
                 * indistinguishable from a dead button, so send the user
                 * somewhere that answers the question they actually have. */
                scene_manager_next_scene(app->scene_manager, GhostTagSceneAbout);
            }
            return true;
        default:
            return false;
        }
    }
    return false;
}

void ghosttag_scene_list_on_exit(void* context) {
    GhostTagApp* app = context;
    device_list_view_set_ok_callback(app->device_list_view, NULL, NULL);
}
