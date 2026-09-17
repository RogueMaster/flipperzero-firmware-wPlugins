#include "../marauder_gui_app_i.h"

/* Formerly two separate top-level WiFi folders ("Ek Araclar" and "Sinyal Analizi") - merged into
   one, since both are non-attack utility/analysis tools rather than anything that targets a
   network, and having 6+ top-level WiFi folders was getting cluttered. */
enum {
    WifiExtraMenuIndexMac,
    WifiExtraMenuIndexSelectAps,
    WifiExtraMenuIndexApInfo,
    WifiExtraMenuIndexPacketCount,
    WifiExtraMenuIndexApFoxHunt,
    WifiExtraMenuIndexStationFoxHunt,
    WifiExtraMenuIndexRecon,
};

static const MarauderMenuItem marauder_wifi_extra_menu_items[] = {
    {"MAC Ayarlari>",
     "MAC Settings>",
     "Rastgele AP/istemci MAC uret veya taranan bir AP/istemcinin MAC'ini klonla.",
     "Generate a random AP/client MAC, or clone a scanned AP's/client's MAC."},
    {"Select APs",
     "Select APs",
     "Birden fazla AP'yi ac/kapa sekilde secili birakir (Marauder'in 'select -a' komutuyla) - saldiri akislarimiz hala tek AP uzerinden calisir, bu ekran sadece secim durumunu yonetir.",
     "Leaves multiple APs toggled on/off selected (via Marauder's 'select -a' command) - our attack flows still work on a single AP, this screen only manages selection state."},
    {"AP Bilgisi",
     "AP Info",
     "Taranan bir AP'nin ESSID, BSSID, kanal, RSSI, guvenlik turu gibi detaylarini gosterir.",
     "Shows a scanned AP's ESSID, BSSID, channel, RSSI, security type and more."},
    {"Paket Sayaci",
     "Packet Counter",
     "Bir AP secip o AG'ye ait yakalanan paket sayisini canli gosterir.",
     "Pick an AP and see the live count of captured packets for that network."},
    {"AP Fox Hunt",
     "AP Fox Hunt",
     "Bir AP secip o AG'den gelen sinyal gucunu (RSSI) canli izleyerek yon bulmani saglar.",
     "Pick an AP and watch its live signal strength (RSSI) to help find its direction."},
    {"Istemci Fox Hunt",
     "Client Fox Hunt",
     "Bir AP altindaki bir istemciyi secip o cihazin sinyal gucunu (RSSI) canli izler.",
     "Pick a client under an AP and watch that device's live signal strength (RSSI)."},
    {"WiFi Kesif",
     "WiFi Recon",
     "WiFi kesif gorevini baslatir (Marauder v1.16.0) ve bulunan AP'leri canli listeler.",
     "Starts the WiFi recon mission (Marauder v1.16.0) and live-lists the APs it finds."},
};

void marauder_gui_scene_wifi_extra_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_wifi_extra_menu_items,
        sizeof(marauder_wifi_extra_menu_items) / sizeof(marauder_wifi_extra_menu_items[0]),
        marauder_gui_text(app, "Analiz&Araclar", "Analysis&Tools"));
}

bool marauder_gui_scene_wifi_extra_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WifiExtraMenuIndexMac) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiMacMenu);
            consumed = true;
        } else if(event.event == WifiExtraMenuIndexSelectAps) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiSelectAps);
            consumed = true;
        } else if(event.event == WifiExtraMenuIndexApInfo) {
            app->wifi_ap_attack_type = 10;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
            consumed = true;
        } else if(event.event == WifiExtraMenuIndexPacketCount) {
            app->wifi_ap_attack_type = 13;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
            consumed = true;
        } else if(event.event == WifiExtraMenuIndexApFoxHunt) {
            app->wifi_ap_attack_type = 14;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
            consumed = true;
        } else if(event.event == WifiExtraMenuIndexStationFoxHunt) {
            app->wifi_ap_attack_type = 15;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
            consumed = true;
        } else if(event.event == WifiExtraMenuIndexRecon) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiRecon);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_wifi_extra_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
