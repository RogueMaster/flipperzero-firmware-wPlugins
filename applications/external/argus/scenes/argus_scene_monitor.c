#include "../argus_i.h"

static void argus_scene_monitor_ok_cb(void* context) {
    ArgusApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, ArgusCustomEventOpenLog);
}

static uint8_t bssid_angle(const uint8_t bssid[6]) {
    /* stable pseudo-random bearing so each AP keeps its spot on the iris */
    return (uint8_t)(bssid[5] * 37 + bssid[4] * 17 + bssid[3] * 7);
}

void argus_scene_monitor_on_enter(void* context) {
    ArgusApp* app = context;

    monitor_view_set_ok_callback(app->monitor_view, argus_scene_monitor_ok_cb, app);
    app->attack_active = false;

    argus_link_arm(app); // start the link + push CHAN / GUARD config

    view_dispatcher_switch_to_view(app->view_dispatcher, ArgusViewMonitor);
}

bool argus_scene_monitor_on_event(void* context, SceneManagerEvent event) {
    ArgusApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ArgusCustomEventTwinFound:
            argus_notify_twin(app);
            consumed = true;
            break;
        case ArgusCustomEventOpenLog:
            scene_manager_next_scene(app->scene_manager, ArgusSceneLog);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        argus_db_tick(app->db, ARGUS_STORM_WINDOW_MS);

        ArgusStats stats;
        argus_db_get_stats(app->db, &stats);

        uint32_t now = furi_get_tick();
        bool connected = app->esp_connected && ((uint32_t)(now - app->last_rx_tick) < 3000);
        bool under_attack = argus_is_under_attack(app, &stats);

        /* build radar blips from the AP table */
        ArgusAp aps[ARGUS_MAX_APS];
        size_t ap_n = argus_db_copy_aps(app->db, aps, ARGUS_MAX_APS);
        MonitorBlip blips[MONITOR_MAX_BLIPS];
        size_t bn = ap_n < MONITOR_MAX_BLIPS ? ap_n : MONITOR_MAX_BLIPS;
        for(size_t i = 0; i < bn; i++) {
            blips[i].angle = bssid_angle(aps[i].bssid);
            blips[i].rssi = aps[i].rssi;
            blips[i].clone = aps[i].clone;
        }

        char guard[ARGUS_SSID_MAX];
        argus_db_get_guard(app->db, guard, sizeof(guard));

        monitor_view_update(
            app->monitor_view,
            blips,
            bn,
            stats.deauth_total,
            stats.deauth_rate,
            stats.ap_count,
            stats.twin_count,
            connected,
            under_attack,
            guard);
        monitor_view_tick(app->monitor_view);

        /* rising edge of an attack -> alarm */
        if(under_attack && !app->attack_active) {
            argus_notify_attack(app);
        }
        app->attack_active = under_attack;

        consumed = true;
    }
    return consumed;
}

void argus_scene_monitor_on_exit(void* context) {
    UNUSED(context);
}
