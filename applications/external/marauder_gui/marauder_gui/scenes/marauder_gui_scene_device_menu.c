#include "../marauder_gui_app_i.h"

enum {
    DeviceMenuIndexSettings,
    DeviceMenuIndexTools,
    DeviceMenuIndexInfo,
    DeviceMenuIndexReboot,
    DeviceMenuIndexLanguage,
    DeviceMenuIndexAbout,
};

static const MarauderMenuItem marauder_device_menu_items[] = {
    {"Ayarlar>",
     "Settings>",
     "Force PMKID, Force Probe, Save Pcap, Enable LED, EP Deauth, Channel Hop ayarlarini ac/kapa.",
     "Toggle Force PMKID, Force Probe, Save Pcap, Enable LED, EP Deauth, Channel Hop settings."},
    {"Araclar>",
     "Tools>",
     "SD listele, firmware guncelle, wardrive yukle, yardim ve WiFi'yi kapat.",
     "List SD, update firmware, upload wardrive, help and shutdown WiFi."},
    {"Cihaz Bilgisi",
     "Device Info",
     "Marauder surumu, donanim, MAC adresleri ve baglantiysa ag bilgisi.",
     "Marauder version, hardware, MAC addresses, and network info if connected."},
    {"Reboot",
     "Reboot",
     "ESP32'yi yeniden baslatir. Seri baglanti birkac saniyeligine kesilir.",
     "Restarts the ESP32. The serial connection will drop for a few seconds."},
    {"Dil",
     "Language",
     "Uygulama dilini Turkce/Ingilizce olarak degistir.",
     "Change the app's language between Turkish/English."},
    {"Hakkinda", "About", "Bu uygulama hakkinda bilgi.", "Information about this app."},
};

void marauder_gui_scene_device_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_device_menu_items,
        sizeof(marauder_device_menu_items) / sizeof(marauder_device_menu_items[0]),
        marauder_gui_text(app, "Cihaz", "Device"));
}

bool marauder_gui_scene_device_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DeviceMenuIndexSettings) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneDeviceSettings);
            consumed = true;
        } else if(event.event == DeviceMenuIndexTools) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneToolsMenu);
            consumed = true;
        } else if(event.event == DeviceMenuIndexInfo) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneDeviceInfo);
            consumed = true;
        } else if(event.event == DeviceMenuIndexReboot) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneDeviceReboot);
            consumed = true;
        } else if(event.event == DeviceMenuIndexLanguage) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneDeviceLanguage);
            consumed = true;
        } else if(event.event == DeviceMenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneDeviceAbout);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_device_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
