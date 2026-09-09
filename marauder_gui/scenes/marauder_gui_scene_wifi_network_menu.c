#include "../marauder_gui_app_i.h"
#include <stdio.h>

enum {
    WifiNetworkMenuIndexJoin,
    WifiNetworkMenuIndexJoinSaved,
    WifiNetworkMenuIndexPingScan,
    WifiNetworkMenuIndexArpScan,
    WifiNetworkMenuIndexPortScan,
};

static const MarauderMenuItem marauder_wifi_network_menu_items[] = {
    {"WiFi'ye Baglan",
     "Connect to WiFi",
     "Taranan aglardan birini secip sifresini girerek o aga baglanir.",
     "Pick one of the scanned networks and enter its password to connect."},
    {"Kayitli Aga Baglan",
     "Connect to Saved Network",
     "Daha once basariyla baglanilan agin kayitli SSID/sifresiyle tekrar baglanir.",
     "Reconnects using the saved SSID/password of a previously successful connection."},
    {"Ping Scan",
     "Ping Scan",
     "Baglanilan agdaki canli (acik) cihazlari IP adresleriyle listeler.",
     "Lists live (reachable) devices on the connected network by IP address."},
    {"ARP Scan",
     "ARP Scan",
     "Baglanilan agdaki cihazlari ARP istekleriyle bulup IP adreslerini listeler.",
     "Finds devices on the connected network via ARP requests and lists their IP addresses."},
    {"Port Scan",
     "Port Scan",
     "Once bir IP secilir, sonra o cihazin acik portlarini (SSH, HTTP, vb.) tarar.",
     "First pick an IP, then it scans that device's open ports (SSH, HTTP, etc.)."},
};

/* Marauder has no CLI command to report live connection status, so this shows our own
   best-known state (set by wifi_join_status.c) - built fresh every time this menu is entered,
   into a `static` buffer since the header pointer must outlive this function (the menu view
   redraws later, independent of this call). */
static char marauder_wifi_network_menu_header[16 + MARAUDER_LINE_MAX];

void marauder_gui_scene_wifi_network_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;

    if(app->wifi_is_connected) {
        snprintf(
            marauder_wifi_network_menu_header,
            sizeof(marauder_wifi_network_menu_header),
            marauder_gui_text(app, "Bagli: %s", "Connected: %s"),
            app->wifi_connected_label);
    } else {
        snprintf(
            marauder_wifi_network_menu_header,
            sizeof(marauder_wifi_network_menu_header),
            "%s",
            /* Kept short: the header is one FontPrimary line and the full
               "Network Tools (Not connected)" ran past the 128px edge. You reach this screen
               through a row literally called "Network Tools", so the state is the useful half. */
            marauder_gui_text(app, "Bagli degil", "Not connected"));
    }

    marauder_gui_menu_set_items(
        app,
        marauder_wifi_network_menu_items,
        sizeof(marauder_wifi_network_menu_items) / sizeof(marauder_wifi_network_menu_items[0]),
        marauder_wifi_network_menu_header);
}

bool marauder_gui_scene_wifi_network_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WifiNetworkMenuIndexJoin) {
            app->wifi_ap_attack_type = 9;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
            consumed = true;
        } else if(event.event == WifiNetworkMenuIndexJoinSaved) {
            app->wifi_join_use_saved = true;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiJoinStatus);
            consumed = true;
        } else if(event.event == WifiNetworkMenuIndexPingScan) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiPingScan);
            consumed = true;
        } else if(event.event == WifiNetworkMenuIndexArpScan) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiArpScan);
            consumed = true;
        } else if(event.event == WifiNetworkMenuIndexPortScan) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiPortScanPickIp);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_wifi_network_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
