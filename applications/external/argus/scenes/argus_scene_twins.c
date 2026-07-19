#include "../argus_i.h"
#include <string.h>

void argus_scene_twins_on_enter(void* context) {
    ArgusApp* app = context;

    char guard[ARGUS_SSID_MAX];
    argus_db_get_guard(app->db, guard, sizeof(guard));
    ap_list_view_set_title(app->ap_list_view, guard[0] ? "Evil Twins" : "Networks");
    ap_list_view_update(app->ap_list_view, NULL, 0, app->esp_connected);

    scene_manager_set_scene_state(app->scene_manager, ArgusSceneTwins, 0);
    argus_link_arm(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ArgusViewApList);
}

bool argus_scene_twins_on_event(void* context, SceneManagerEvent event) {
    ArgusApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ArgusCustomEventTwinFound) {
            argus_notify_twin(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        uint32_t now = furi_get_tick();
        bool connected = app->esp_connected && ((uint32_t)(now - app->last_rx_tick) < 3000);

        char guard[ARGUS_SSID_MAX];
        argus_db_get_guard(app->db, guard, sizeof(guard));

        ArgusAp aps[ARGUS_MAX_APS];
        size_t n = argus_db_copy_aps(app->db, aps, ARGUS_MAX_APS);

        ApRow rows[ARGUS_MAX_APS];
        size_t r = 0;
        for(size_t i = 0; i < n; i++) {
            bool match = (guard[0] == '\0') || (strncmp(aps[i].ssid, guard, ARGUS_SSID_MAX) == 0);
            if(!match) continue;
            strncpy(rows[r].ssid, aps[i].ssid, sizeof(rows[r].ssid) - 1);
            rows[r].ssid[sizeof(rows[r].ssid) - 1] = '\0';
            memcpy(rows[r].bssid, aps[i].bssid, 6);
            rows[r].channel = aps[i].channel;
            rows[r].rssi = aps[i].rssi;
            rows[r].enc = (uint8_t)aps[i].enc;
            rows[r].clone = aps[i].clone;
            r++;
        }
        ap_list_view_update(app->ap_list_view, rows, r, connected);

        /* notify on a freshly grown twin count */
        ArgusStats stats;
        argus_db_get_stats(app->db, &stats);
        uint32_t prev = scene_manager_get_scene_state(app->scene_manager, ArgusSceneTwins);
        if(stats.twin_count > prev) argus_notify_twin(app);
        scene_manager_set_scene_state(app->scene_manager, ArgusSceneTwins, stats.twin_count);

        consumed = true;
    }
    return consumed;
}

void argus_scene_twins_on_exit(void* context) {
    UNUSED(context);
}
