#include "../marauder_gui_app_i.h"

enum {
    WifiMacMenuIndexRandomAp,
    WifiMacMenuIndexRandomSta,
    WifiMacMenuIndexCloneAp,
    WifiMacMenuIndexCloneSta,
};

static const MarauderMenuItem marauder_wifi_mac_menu_items[] = {
    {"Rastgele AP MAC",
     "Random AP MAC",
     "ESP32'nin kendi sahte-AP MAC adresini rastgele degistirir (randapmac).",
     "Randomizes the ESP32's own fake-AP MAC address (randapmac)."},
    {"Rastgele Istemci MAC",
     "Random Client MAC",
     "ESP32'nin WiFi istemci (STA) MAC adresini rastgele degistirir (randstamac).",
     "Randomizes the ESP32's WiFi client (STA) MAC address (randstamac)."},
    {"AP MAC Klonla",
     "Clone AP MAC",
     "Taranan bir AP'yi secip onun BSSID'sini kendi sahte-AP MAC'in yapar (cloneapmac).",
     "Pick a scanned AP and make its BSSID your own fake-AP MAC (cloneapmac)."},
    {"Istemci MAC Klonla",
     "Clone Client MAC",
     "Bir AP altindaki bir istemciyi secip onun MAC'ini kendi istemci MAC'in yapar (clonestamac).",
     "Pick a client under an AP and make its MAC your own client MAC (clonestamac)."},
};

void marauder_gui_scene_wifi_mac_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_wifi_mac_menu_items,
        sizeof(marauder_wifi_mac_menu_items) / sizeof(marauder_wifi_mac_menu_items[0]),
        marauder_gui_text(app, "MAC Ayarlari", "MAC Settings"));
}

bool marauder_gui_scene_wifi_mac_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WifiMacMenuIndexRandomAp) {
            app->wifi_mac_random_is_ap = true;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiRandomMac);
            consumed = true;
        } else if(event.event == WifiMacMenuIndexRandomSta) {
            app->wifi_mac_random_is_ap = false;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiRandomMac);
            consumed = true;
        } else if(event.event == WifiMacMenuIndexCloneAp) {
            app->wifi_ap_attack_type = 12;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
            consumed = true;
        } else if(event.event == WifiMacMenuIndexCloneSta) {
            app->wifi_ap_attack_type = 11;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_wifi_mac_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
