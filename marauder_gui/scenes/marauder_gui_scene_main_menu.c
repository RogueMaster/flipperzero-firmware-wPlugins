#include "../marauder_gui_app_i.h"

enum {
    MainMenuIndexWifi,
    MainMenuIndexBt,
    MainMenuIndexGps,
    MainMenuIndexDevice,
    MainMenuIndexTerminal,
};

static const MarauderMenuItem marauder_main_menu_items[] = {
    {"WiFi>",
     "WiFi>",
     "WiFi tarama, sahte ag olusturma, saldiri ve dedektor ozelliklerinin tamami.",
     "All WiFi scanning, fake network creation, attack and detector features."},
    {"Bluetooth>",
     "Bluetooth>",
     "Bluetooth spam, tracker bulma ve BLE dedektor ozellikleri.",
     "Bluetooth spam, tracker finding and BLE detector features."},
    {"GPS>",
     "GPS>",
     "GPS verisi, NMEA akisi, takip ve POI - ESP32'ye bagli bir GPS modulu gerektirir.",
     "GPS data, NMEA stream, tracker and POI - needs a GPS module wired to the ESP32."},
    {"Cihaz>",
     "Device>",
     "Cihaz bilgisi, Marauder ayarlari (Force PMKID, Channel Hop, vb.) ve Reboot.",
     "Device info, Marauder settings (Force PMKID, Channel Hop, etc.) and Reboot."},
    {"Terminal",
     "Terminal",
     "Marauder'a istedigin ham komutu yaz, tam cevabini gor. Hata ayiklama icin kullanislidir.",
     "Type any raw command to Marauder and see the full reply. Useful for debugging."},
};

void marauder_gui_scene_main_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_main_menu_items,
        sizeof(marauder_main_menu_items) / sizeof(marauder_main_menu_items[0]),
        "Marauder GUI");
}

bool marauder_gui_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MainMenuIndexWifi) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiMenu);
            consumed = true;
        } else if(event.event == MainMenuIndexBt) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtMenu);
            consumed = true;
        } else if(event.event == MainMenuIndexGps) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneGpsMenu);
            consumed = true;
        } else if(event.event == MainMenuIndexDevice) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneDeviceMenu);
            consumed = true;
        } else if(event.event == MainMenuIndexTerminal) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneTerminalInput);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Root scene: Back exits the app */
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_main_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
