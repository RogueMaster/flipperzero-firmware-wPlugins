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

    // picking from the list is also the only way to get an SSID with odd
    // characters in it right
    wol_strcpy(app->config.ssid, WOL_SSID_LEN, app->scan_list[event.event].ssid);
    wol_config_save(&app->config);

    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, WolSceneWifi);
    return true;
}

void wol_scene_wifi_scan_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}
