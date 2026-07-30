#include "wol_flipper.h"

static char scan_labels[WOL_SSID_MAX_SCAN][WOL_SSID_LEN + 10];

static void wol_scene_wifi_scan_callback(void* context, uint32_t index) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void wol_scene_wifi_scan_on_enter(void* context) {
    WolApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->scan_count ? "Pick a network" : "Nothing found");

    for(size_t i = 0; i < app->scan_count; i++) {
        snprintf(
            scan_labels[i],
            sizeof(scan_labels[i]),
            "%s %d",
            app->scan_list[i].ssid,
            app->scan_list[i].rssi);
        submenu_add_item(app->submenu, scan_labels[i], i, wol_scene_wifi_scan_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewSubmenu);
}

bool wol_scene_wifi_scan_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= app->scan_count) return false;

    /* Picking from the list is also the only way to get an SSID with odd
     * characters in it right. An already saved network opens for editing, a new
     * one goes straight to its key. */
    const char* ssid = app->scan_list[event.event].ssid;
    uint8_t existing = wol_config_find_network(&app->config, ssid);

    if(existing < WOL_MAX_NETWORKS) {
        app->network_is_new = false;
        app->network_index = existing;
        app->edit_network = app->config.networks[existing];
    } else {
        app->network_is_new = true;
        app->network_index = app->config.network_count;
        memset(&app->edit_network, 0, sizeof(WolNetwork));
        wol_strcpy(app->edit_network.ssid, WOL_SSID_LEN, ssid);
    }

    scene_manager_next_scene(app->scene_manager, WolSceneNetworkEdit);
    return true;
}

void wol_scene_wifi_scan_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}
