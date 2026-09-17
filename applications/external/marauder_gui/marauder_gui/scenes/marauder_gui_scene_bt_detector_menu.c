#include "../marauder_gui_app_i.h"

enum {
    BtDetectorMenuIndexGeneral,
    BtDetectorMenuIndexFlipper,
    BtDetectorMenuIndexMeta,
    BtDetectorMenuIndexSkimmer,
    BtDetectorMenuIndexFlock,
};

static const MarauderMenuItem marauder_bt_detector_menu_items[] = {
    {"Genel BLE Tara",
     "General BLE Scan",
     "Yakindaki tum Bluetooth LE cihazlarini isim ve sinyal gucuyle listeler.",
     "Lists all nearby Bluetooth LE devices with name and signal strength."},
    {"Flipper Bulucu",
     "Flipper Finder",
     "Yakinda baska bir Flipper Zero cihazi olup olmadigini bulur.",
     "Finds whether another Flipper Zero device is nearby."},
    {"Meta Gozluk Dedektoru",
     "Meta Glasses Detector",
     "Gizli kayit yapabilen Meta/Ray-Ban akilli gozlukleri tespit eder.",
     "Detects Meta/Ray-Ban smart glasses capable of covert recording."},
    {"Skimmer Tespiti",
     "Skimmer Detection",
     "Kredi karti bilgisi calan bilinen Bluetooth 'skimmer' modullerini (HC-03/05/06) tespit eder.",
     "Detects known Bluetooth 'skimmer' modules (HC-03/05/06) that steal credit card data."},
    {"Flock Kamera Dedektoru",
     "Flock Camera Detector",
     "Sokaklarda yayginlasan Flock Safety plaka/gozetim kameralarini tespit eder.",
     "Detects the increasingly common Flock Safety license-plate/surveillance cameras."},
};

void marauder_gui_scene_bt_detector_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_bt_detector_menu_items,
        sizeof(marauder_bt_detector_menu_items) / sizeof(marauder_bt_detector_menu_items[0]),
        marauder_gui_text(app, "BT Dedektorleri", "BT Detectors"));
}

bool marauder_gui_scene_bt_detector_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtDetectorMenuIndexGeneral) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtDetectGeneral);
            consumed = true;
        } else if(event.event == BtDetectorMenuIndexFlipper) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtDetectFlipper);
            consumed = true;
        } else if(event.event == BtDetectorMenuIndexMeta) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtDetectMeta);
            consumed = true;
        } else if(event.event == BtDetectorMenuIndexSkimmer) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtDetectSkimmer);
            consumed = true;
        } else if(event.event == BtDetectorMenuIndexFlock) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtDetectFlock);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_bt_detector_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
