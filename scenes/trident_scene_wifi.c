#include "../trident_i.h"

typedef enum {
    WifiScanAp,
    WifiScanSta,
    WifiSigmon,
    WifiSetChannel,
    WifiTarget,
    WifiSniffBeacon,
    WifiSniffProbe,
    WifiSniffDeauth,
    WifiSniffPmkid,
    WifiSniffPwn,
    WifiSniffEsp,
    WifiSniffRaw,
    WifiSsidList,
    WifiAttacks,
} WifiIndex;

static void trident_scene_wifi_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_wifi_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "Wi-Fi");
    submenu_add_item(menu, "Scan APs", WifiScanAp, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Scan Stations", WifiScanSta, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Channel Analyzer", WifiSigmon, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Set Channel", WifiSetChannel, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Targets / Select", WifiTarget, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Sniff Beacons", WifiSniffBeacon, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Sniff Probes", WifiSniffProbe, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Sniff Deauth", WifiSniffDeauth, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Sniff PMKID", WifiSniffPmkid, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Sniff Pwnagotchi", WifiSniffPwn, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Sniff ESP", WifiSniffEsp, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Sniff Raw", WifiSniffRaw, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "SSID List", WifiSsidList, trident_scene_wifi_cb, app);
    submenu_add_item(menu, "Attacks", WifiAttacks, trident_scene_wifi_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneWifi));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_wifi_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneWifi, event.event);
        consumed = true;
        switch(event.event) {
        case WifiScanAp:
            trident_launch(app, "Scan APs", MARAUDER_CMD_SCAN_AP, false);
            break;
        case WifiScanSta:
            trident_launch(app, "Scan Stations", MARAUDER_CMD_SCAN_STA, false);
            break;
        case WifiSigmon:
            trident_launch(app, "Channel Analyzer", MARAUDER_CMD_SIGMON, false);
            break;
        case WifiSetChannel:
            trident_prompt(
                app, "Wi-Fi channel (e.g. 6, 36)", MARAUDER_PFX_CHANNEL, "Channel",
                MARAUDER_CMD_CHANNEL);
            break;
        case WifiTarget:
            scene_manager_next_scene(app->scene_manager, TridentSceneTarget);
            break;
        case WifiSniffBeacon:
            trident_launch(app, "Sniff Beacons", MARAUDER_CMD_SNIFF_BEACON, false);
            break;
        case WifiSniffProbe:
            trident_launch(app, "Sniff Probes", MARAUDER_CMD_SNIFF_PROBE, false);
            break;
        case WifiSniffDeauth:
            trident_launch(app, "Sniff Deauth", MARAUDER_CMD_SNIFF_DEAUTH, false);
            break;
        case WifiSniffPmkid:
            trident_launch(app, "Sniff PMKID", MARAUDER_CMD_SNIFF_PMKID, false);
            break;
        case WifiSniffPwn:
            trident_launch(app, "Sniff Pwnagotchi", MARAUDER_CMD_SNIFF_PWN, false);
            break;
        case WifiSniffEsp:
            trident_launch(app, "Sniff ESP", MARAUDER_CMD_SNIFF_ESP, false);
            break;
        case WifiSniffRaw:
            trident_launch(app, "Sniff Raw", MARAUDER_CMD_SNIFF_RAW, false);
            break;
        case WifiSsidList:
            scene_manager_next_scene(app->scene_manager, TridentSceneSsidlist);
            break;
        case WifiAttacks:
            scene_manager_next_scene(app->scene_manager, TridentSceneAttacks);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_wifi_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
