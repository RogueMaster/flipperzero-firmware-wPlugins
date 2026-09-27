#include "../ghosttag_i.h"

#define SCAN_RADAR_BLIPS 24

/* A stable but arbitrary dial position per device. It is NOT a bearing -
 * GhostTag has no antenna array and cannot know one. Hashing the address just
 * keeps each tracker in its own spot so you can watch it move in and out. */
static uint8_t mac_slot(const uint8_t mac[6]) {
    return (uint8_t)(mac[0] * 31u + mac[3] * 7u + mac[5]);
}

static void ghosttag_scene_scan_ok_cb(void* context) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventScanOpenList);
}

static void ghosttag_scene_scan_help_cb(void* context) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventScanOpenHelp);
}

void ghosttag_scene_scan_on_enter(void* context) {
    GhostTagApp* app = context;

    radar_view_set_ok_callback(app->radar_view, ghosttag_scene_scan_ok_cb, app);
    radar_view_set_help_callback(app->radar_view, ghosttag_scene_scan_help_cb, app);

    /* The source is started by whoever pushed us (the main menu), and it keeps
     * running while the user dips into the list and detail screens. Re-entering
     * this scene must not restart it - that would wipe the detections the user
     * just walked back from looking at. */
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewRadar);
}

bool ghosttag_scene_scan_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        /* An alert takes priority over redrawing the dial. */
        if(ghosttag_poll_alert(app)) return true;

        RadarLinkState link;
        if(app->source == GhostTagSourceDemo) {
            link = RadarLinkDemo;
        } else {
            ghosttag_update_link(app);
            if(app->esp_connected) {
                link = RadarLinkUp;
            } else {
                /* "Never heard a board" and "the board stopped talking" are
                 * different problems with different fixes, so they get
                 * different words. */
                link = app->esp_ever_seen ? RadarLinkLost : RadarLinkWaiting;
            }
        }

        TrackerRecord* snap = app->scratch;
        size_t n = tracker_db_snapshot(app->db, snap, SCAN_RADAR_BLIPS);

        RadarBlip blips[SCAN_RADAR_BLIPS];
        for(size_t i = 0; i < n; i++) {
            blips[i].rssi = snap[i].rssi;
            blips[i].slot = mac_slot(snap[i].mac);
            blips[i].following = snap[i].following;
            blips[i].threat = tracker_type_is_threat(snap[i].type);
        }

        radar_view_set_data(
            app->radar_view,
            blips,
            n,
            tracker_db_present_count(app->db),
            tracker_db_threat_count(app->db),
            tracker_db_following_count(app->db),
            link);
        radar_view_tick(app->radar_view);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GhostTagCustomEventScanOpenList:
            scene_manager_next_scene(app->scene_manager, GhostTagSceneList);
            return true;
        case GhostTagCustomEventScanOpenHelp:
            scene_manager_next_scene(app->scene_manager, GhostTagSceneAbout);
            return true;
        default:
            return false;
        }
    }
    return false;
}

void ghosttag_scene_scan_on_exit(void* context) {
    GhostTagApp* app = context;
    /* Scanning deliberately continues while the user browses list/detail; the
     * session is torn down by the main menu's on_enter, which is the only
     * place the user can actually leave a hunt from. */
    radar_view_set_ok_callback(app->radar_view, NULL, NULL);
    radar_view_set_help_callback(app->radar_view, NULL, NULL);
}
