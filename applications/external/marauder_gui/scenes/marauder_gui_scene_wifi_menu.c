#include "../marauder_gui_app_i.h"

/* All 5 items here are folders (submenu, label ending in ">") - "AP Tara ve Deauth" and "Karma
   Attack" used to sit here as standalone actions, but were folded into "Saldirilar>" (Deauth is
   now its first item, Evil Portal/Karma its last) so this menu doesn't mix loose actions in with
   its folders. Enum order must match marauder_wifi_menu_items' order exactly since Ok reports
   the array index directly. */
enum {
    WifiMenuIndexSpam,
    WifiMenuIndexAttacks,
    WifiMenuIndexDetectors,
    WifiMenuIndexNetwork,
    WifiMenuIndexExtra,
};

static const MarauderMenuItem marauder_wifi_menu_items[] = {
    {"WiFi Spam>",
     "WiFi Spam>",
     "Rastgele, Rick Roll, komik veya sayisi belirlenmis sahte WiFi aglari yayinlar.",
     "Broadcasts random, Rick Roll, funny, or a fixed number of fake WiFi networks."},
    {"Saldirilar>",
     "Attacks>",
     "Deauth, Hedefli Deauth, Probe Flood, AP Klon, SAE, CSA, Quiet Time, Bad Msg, Assoc Sleep ve Evil Portal (Karma).",
     "Deauth, Targeted Deauth, Probe Flood, AP Clone, SAE, CSA, Quiet Time, Bad Msg, Assoc Sleep and Evil Portal (Karma)."},
    {"Monitor&Sniff>",
     "Monitor&Sniff>",
     "Pineapple, MultiSSID, Pwnagotchi, Deauth Sniff ve MAC Monitor - pasif dinleyici/tespit araclari.",
     "Pineapple, MultiSSID, Pwnagotchi, Deauth Sniff and MAC Monitor - passive listening/detection tools."},
    {"Ag Araclari>",
     "Network Tools>",
     "Bir WiFi agina baglan (sifreyle veya kayitli bilgilerle).",
     "Connect to a WiFi network (with a password or saved credentials)."},
    {"Analiz&Araclar>",
     "Analysis&Tools>",
     "Coklu AP secimi, AP bilgisi, MAC ayarlari, paket sayaci ve Fox Hunt (sinyal gucuyle yon bulma).",
     "Multi-AP selection, AP info, MAC settings, packet counter and Fox Hunt (signal-strength direction finding)."},
};

void marauder_gui_scene_wifi_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_wifi_menu_items,
        sizeof(marauder_wifi_menu_items) / sizeof(marauder_wifi_menu_items[0]),
        "WiFi");
}

bool marauder_gui_scene_wifi_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WifiMenuIndexSpam) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiSpamMenu);
            consumed = true;
        } else if(event.event == WifiMenuIndexAttacks) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiAttackMenu);
            consumed = true;
        } else if(event.event == WifiMenuIndexDetectors) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiDetectorMenu);
            consumed = true;
        } else if(event.event == WifiMenuIndexNetwork) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiNetworkMenu);
            consumed = true;
        } else if(event.event == WifiMenuIndexExtra) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiExtraMenu);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_wifi_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
