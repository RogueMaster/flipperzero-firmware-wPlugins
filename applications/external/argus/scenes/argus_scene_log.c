#include "../argus_i.h"
#include <string.h>

void argus_scene_log_on_enter(void* context) {
    ArgusApp* app = context;
    threat_log_view_update(app->threat_log_view, NULL, 0, app->esp_connected);
    view_dispatcher_switch_to_view(app->view_dispatcher, ArgusViewThreatLog);
}

bool argus_scene_log_on_event(void* context, SceneManagerEvent event) {
    ArgusApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        uint32_t now = furi_get_tick();
        bool connected = app->esp_connected && ((uint32_t)(now - app->last_rx_tick) < 3000);

        ArgusThreat log[ARGUS_MAX_LOG];
        size_t n = argus_db_copy_log(app->db, log, ARGUS_MAX_LOG);

        LogRow rows[ARGUS_MAX_LOG];
        for(size_t i = 0; i < n; i++) {
            rows[i].kind = (uint8_t)log[i].kind;
            strncpy(rows[i].ssid, log[i].ssid, sizeof(rows[i].ssid) - 1);
            rows[i].ssid[sizeof(rows[i].ssid) - 1] = '\0';
            rows[i].channel = log[i].channel;
            rows[i].rssi = log[i].rssi;
            rows[i].reason = log[i].reason;
            rows[i].age_s = (uint32_t)(now - log[i].time_tick) / 1000u;
        }
        threat_log_view_update(app->threat_log_view, rows, n, connected);
        consumed = true;
    }
    return consumed;
}

void argus_scene_log_on_exit(void* context) {
    UNUSED(context);
}
