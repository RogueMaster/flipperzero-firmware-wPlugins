#include "../marauder_gui_app_i.h"

enum {
    WifiDetectorMenuIndexPcapSniff,
    WifiDetectorMenuIndexPineapple,
    WifiDetectorMenuIndexMultiSSID,
    WifiDetectorMenuIndexPwnagotchi,
    WifiDetectorMenuIndexDeauthSniff,
    WifiDetectorMenuIndexMactrack,
};

static const MarauderMenuItem marauder_wifi_detector_menu_items[] = {
    {"PCAP Kayit>",
     "PCAP Capture>",
     "Cerceveleri (raw/beacon/deauth/probe/pmkid/pwn) yakalayip SD karta .pcap olarak kaydeder.",
     "Captures frames (raw/beacon/deauth/probe/pmkid/pwn) and saves them to SD as .pcap."},
    {"Pineapple Tespiti",
     "Pineapple Detection",
     "Sahte AP kurup araya giren (MITM) 'WiFi Pineapple' cihazlarini tespit eder.",
     "Detects 'WiFi Pineapple' man-in-the-middle devices that set up fake APs."},
    {"MultiSSID Tespiti",
     "MultiSSID Detection",
     "Anormal sekilde cok fazla SSID yayinlayan supheli/kurcalanmis cihazlari tespit eder.",
     "Detects suspicious/tampered devices broadcasting an abnormally large number of SSIDs."},
    {"Pwnagotchi Tespiti",
     "Pwnagotchi Detection",
     "El sikismasi toplayan Pwnagotchi cihazlarini, ismi ve topladigi sayiyla gosterir.",
     "Shows handshake-collecting Pwnagotchi devices, with their name and collected count."},
    {"Deauth Sniff",
     "Deauth Sniff",
     "Havada ucusan deauth/baglanti kesme paketlerini canli olarak dinler ve listeler.",
     "Listens for and lists deauth/disconnect packets flying through the air, live."},
    {"MAC Monitor",
     "MAC Monitor",
     "Yakinda surekli gorunen (seni takip ediyor olabilecek) cihazlari tespit eder.",
     "Detects devices that keep appearing nearby (which may be following/tracking you)."},
};

void marauder_gui_scene_wifi_detector_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_wifi_detector_menu_items,
        sizeof(marauder_wifi_detector_menu_items) / sizeof(marauder_wifi_detector_menu_items[0]),
        "Monitor&Sniff");
}

bool marauder_gui_scene_wifi_detector_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WifiDetectorMenuIndexPineapple) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiDetectPineapple);
            consumed = true;
        } else if(event.event == WifiDetectorMenuIndexMultiSSID) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiDetectMultiSSID);
            consumed = true;
        } else if(event.event == WifiDetectorMenuIndexPwnagotchi) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiDetectPwnagotchi);
            consumed = true;
        } else if(event.event == WifiDetectorMenuIndexDeauthSniff) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiDetectDeauth);
            consumed = true;
        } else if(event.event == WifiDetectorMenuIndexMactrack) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiDetectMactrack);
            consumed = true;
        } else if(event.event == WifiDetectorMenuIndexPcapSniff) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapSniffMenu);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_wifi_detector_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
