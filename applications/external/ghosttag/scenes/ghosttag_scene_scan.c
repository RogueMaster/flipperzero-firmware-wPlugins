#include "../ghosttag_i.h"

#define SCAN_RADAR_BLIPS 24
#define ESP_TIMEOUT_MS 4000

static void ghosttag_scene_scan_ok_cb(void* context) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventScanOpenList);
}

static uint8_t mac_bearing(const uint8_t mac[6]) {
    return (uint8_t)(mac[0] * 31u + mac[3] * 7u + mac[5]);
}

void ghosttag_scene_scan_on_enter(void* context) {
    GhostTagApp* app = context;

    radar_view_set_ok_callback(app->radar_view, ghosttag_scene_scan_ok_cb, app);

    // Start a fresh hunt only when no session is already live (we keep scanning
    // while the user dips into the list / detail screens).
    if(!uart_link_is_running(app->uart)) {
        tracker_db_reset(app->db);
        app->last_rx_tick = 0;
        app->esp_connected = false;
        uart_link_start(app->uart);
        uart_link_send_command(app->uart, "START\n");
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewRadar);
}

bool ghosttag_scene_scan_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        app->esp_connected = (furi_get_tick() - app->last_rx_tick) < ESP_TIMEOUT_MS;

        TrackerRecord snap[SCAN_RADAR_BLIPS];
        size_t n = tracker_db_snapshot(app->db, snap, SCAN_RADAR_BLIPS);

        RadarBlip blips[SCAN_RADAR_BLIPS];
        for(size_t i = 0; i < n; i++) {
            blips[i].rssi = snap[i].rssi;
            blips[i].angle = mac_bearing(snap[i].mac);
            blips[i].following = snap[i].following;
        }

        radar_view_set_data(
            app->radar_view,
            blips,
            n,
            tracker_db_count(app->db),
            tracker_db_following_count(app->db),
            app->esp_connected);
        radar_view_tick(app->radar_view);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GhostTagCustomEventScanOpenList:
            scene_manager_next_scene(app->scene_manager, GhostTagSceneList);
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

void ghosttag_scene_scan_on_exit(void* context) {
    UNUSED(context);
    // Scanning intentionally continues while browsing list/detail; it is torn
    // down in the Start scene when the user leaves the hunt.
}
