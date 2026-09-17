#include "../marauder_gui_app_i.h"
#include <string.h>

/* Miscellaneous single-command Marauder tools. Order must match marauder_tools_menu_items below
   (the menu fires the row index as its custom event). Each row runs the command and shows its
   output via the shared cmd_output scene. */
static const char* const marauder_tools_commands[] = {
    "ls /", /* List SD (ESP32 side) */
    "update -s", /* Update firmware from ESP32 SD */
    "upload -d wigle", /* Upload wardrive logs to WiGLE */
    "upload -d both", /* Upload wardrive logs (WiGLE + WiGLE DB) */
    "help", /* Command help */
    "stopscan -f", /* Force-shutdown WiFi */
    /* SPIFFS backup/restore + protocol info, added in Marauder firmware v1.16.0 */
    "backupspiffs",
    "backupstatus",
    "restorespiffs",
    "protocolinfo",
};

static const MarauderMenuItem marauder_tools_menu_items[] = {
    {"SD Listele",
     "List SD",
     "ESP32'nin SD kartinin kok dizinini listeler.",
     "Lists the root directory of the ESP32's SD card."},
    {"Firmware Guncelle",
     "Update Firmware",
     "ESP32 Marauder'i kendi SD kartindan gunceller.",
     "Updates ESP32 Marauder from its own SD card."},
    {"Wardrive Yukle: WiGLE",
     "Upload Wardrive: WiGLE",
     "Kaydedilen wardrive verilerini WiGLE'a yukler.",
     "Uploads saved wardrive data to WiGLE."},
    {"Wardrive Yukle: Ikisi",
     "Upload Wardrive: Both",
     "Wardrive verilerini WiGLE ve WiGLE DB'ye yukler.",
     "Uploads wardrive data to both WiGLE and WiGLE DB."},
    {"Yardim", "Help", "Marauder'in komut listesini gosterir.", "Shows Marauder's command list."},
    {"WiFi'yi Kapat",
     "Shutdown WiFi",
     "Tum taramalari durdurur ve ESP32'nin WiFi surucusunu kapatir.",
     "Stops all scans and shuts down the ESP32's WiFi driver."},
    {"SPIFFS Yedekle",
     "Backup SPIFFS",
     "ESP32'nin SPIFFS icerigini SD kartta /spiffs klasorune kopyalar.",
     "Copies the ESP32's SPIFFS content to /spiffs on its SD card."},
    {"Yedek Durumu",
     "Backup Status",
     "SD karttaki /spiffs yedegini inceler.",
     "Inspects the /spiffs backup on the SD card."},
    {"SPIFFS Geri Yukle",
     "Restore SPIFFS",
     "SD karttaki /spiffs yedeginden SPIFFS icerigini geri yukler.",
     "Restores SPIFFS content from the /spiffs backup on the SD card."},
    {"Protokol Bilgisi",
     "Protocol Info",
     "Marauder'in makine protokolu hakkinda bilgi verir.",
     "Reports information about Marauder's machine protocol."},
};

void marauder_gui_scene_tools_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_tools_menu_items,
        sizeof(marauder_tools_menu_items) / sizeof(marauder_tools_menu_items[0]),
        marauder_gui_text(app, "Araclar", "Tools"));
}

bool marauder_gui_scene_tools_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        size_t count = sizeof(marauder_tools_commands) / sizeof(marauder_tools_commands[0]);
        if(event.event < count) {
            strncpy(
                app->terminal_cmd,
                marauder_tools_commands[event.event],
                sizeof(app->terminal_cmd) - 1);
            app->terminal_cmd[sizeof(app->terminal_cmd) - 1] = '\0';
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneCmdOutput);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_tools_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
