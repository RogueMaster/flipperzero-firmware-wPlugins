#include "../marauder_gui_app_i.h"

/* Folders (submenu items, label ending in ">") listed first, then actions - enum order must
   match marauder_bt_menu_items' order exactly since Ok reports the array index directly. */
enum {
    BtMenuIndexSpam,
    BtMenuIndexDetectors,
    BtMenuIndexTracker,
    BtMenuIndexRecon,
};

static const MarauderMenuItem marauder_bt_menu_items[] = {
    {"BLE Spam>",
     "BLE Spam>",
     "iOS, Android, Windows ve Samsung cihazlarina sahte eslesme bildirimleri gonderir.",
     "Sends fake pairing notifications to iOS, Android, Windows and Samsung devices."},
    {"Dedektorler>",
     "Detectors>",
     "Genel BLE tarama, Flipper bulucu, Meta gozluk, skimmer ve Flock kamera dedektorleri.",
     "General BLE scan, Flipper finder, Meta glasses, skimmer and Flock camera detectors."},
    {"AirTag/Tracker Bul",
     "AirTag/Tracker Finder",
     "Yakindaki AirTag/Find My cihazlarini bulur; birini secip ses caldirabilir veya kimligini taklit edebilirsin (spoof).",
     "Finds nearby AirTag/Find My devices; pick one to play a sound or spoof its identity."},
    {"BLE Kesif",
     "BLE Recon",
     "BLE kesif gorevini baslatir (Marauder v1.16.0) ve bulunan cihazlari canli listeler.",
     "Starts the BLE recon mission (Marauder v1.16.0) and live-lists the devices it finds."},
};

void marauder_gui_scene_bt_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_bt_menu_items,
        sizeof(marauder_bt_menu_items) / sizeof(marauder_bt_menu_items[0]),
        "Bluetooth");
}

bool marauder_gui_scene_bt_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtMenuIndexSpam) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtSpamMenu);
            consumed = true;
        } else if(event.event == BtMenuIndexTracker) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtTrackerScan);
            consumed = true;
        } else if(event.event == BtMenuIndexDetectors) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtDetectorMenu);
            consumed = true;
        } else if(event.event == BtMenuIndexRecon) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtRecon);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_bt_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
