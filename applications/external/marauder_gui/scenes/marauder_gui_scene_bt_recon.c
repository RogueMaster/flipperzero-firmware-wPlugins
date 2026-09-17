#include "../marauder_gui_app_i.h"

/* BLE half of Marauder's cross-hardware "Recon" mission mode - see wifi_recon.c for why this is
   a live-list dashboard (reusing the same helper as bt_detect_general.c) rather than a raw
   "recon status" readout: "recon ble" runs the exact same BT_SCAN_ALL scan sniffbt does, so
   "list -b" (the same "[N][RSSI:x] name" lines the general BLE detector already parses) works
   identically while a Recon mission is running. */
void marauder_gui_scene_bt_recon_on_enter(void* context) {
    MarauderGuiApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);
    marauder_gui_list_scan_start(
        app,
        "recon ble",
        "list -b",
        marauder_gui_text(app, "BLE Kesif Calisiyor..", "BLE Recon Running.."),
        marauder_gui_text(app, "BLE cihazi bulunamadi...", "No BLE device found..."));
}

bool marauder_gui_scene_bt_recon_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_list_scan_handle_back(context, event);
}

void marauder_gui_scene_bt_recon_on_exit(void* context) {
    marauder_gui_list_scan_exit(context);
}
