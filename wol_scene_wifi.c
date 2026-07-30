#include "wol_flipper.h"

#include <string.h>

/* Saved networks occupy indices 0..count-1, the actions sit above them. */
#define WIFI_INDEX_SCAN 0xF0
#define WIFI_INDEX_ADD  0xF1
#define WIFI_INDEX_TEST 0xF2

static char wifi_labels[WOL_MAX_NETWORKS][WOL_SSID_LEN + 8];

static void wol_scene_wifi_callback(void* context, uint32_t index) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void wol_scene_wifi_on_enter(void* context) {
    WolApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Wi-Fi networks");

    for(size_t i = 0; i < app->config.network_count; i++) {
        const WolNetwork* network = &app->config.networks[i];
        snprintf(
            wifi_labels[i],
            sizeof(wifi_labels[i]),
            "%s%s",
            network->ssid,
            network->pass[0] ? "" : " (open)");
        submenu_add_item(app->submenu, wifi_labels[i], i, wol_scene_wifi_callback, app);
    }

    submenu_add_item(
        app->submenu, "Scan for networks", WIFI_INDEX_SCAN, wol_scene_wifi_callback, app);
    if(app->config.network_count < WOL_MAX_NETWORKS) {
        submenu_add_item(
            app->submenu, "Add by name", WIFI_INDEX_ADD, wol_scene_wifi_callback, app);
    }
    if(app->config.network_count) {
        submenu_add_item(
            app->submenu, "Test connection", WIFI_INDEX_TEST, wol_scene_wifi_callback, app);
    }

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, WolSceneWifi));
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewSubmenu);
}

bool wol_scene_wifi_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    /* The action codes are not submenu positions, so park the cursor on the
     * first action instead of feeding a bogus index back to the submenu. */
    if(event.event >= WIFI_INDEX_SCAN) {
        scene_manager_set_scene_state(app->scene_manager, WolSceneWifi, app->config.network_count);
    }

    switch(event.event) {
    case WIFI_INDEX_SCAN:
        app->wake_op = WolWakeOpScan;
        scene_manager_next_scene(app->scene_manager, WolSceneSend);
        return true;

    case WIFI_INDEX_ADD:
        app->network_is_new = true;
        app->network_index = app->config.network_count;
        memset(&app->edit_network, 0, sizeof(WolNetwork));
        scene_manager_next_scene(app->scene_manager, WolSceneNetworkEdit);
        return true;

    case WIFI_INDEX_TEST:
        app->wake_op = WolWakeOpWifiTest;
        scene_manager_next_scene(app->scene_manager, WolSceneSend);
        return true;

    default:
        break;
    }

    if(event.event < app->config.network_count) {
        scene_manager_set_scene_state(app->scene_manager, WolSceneWifi, event.event);
        app->network_is_new = false;
        app->network_index = event.event;
        app->edit_network = app->config.networks[app->network_index];
        scene_manager_next_scene(app->scene_manager, WolSceneNetworkEdit);
        return true;
    }

    return false;
}

void wol_scene_wifi_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}
