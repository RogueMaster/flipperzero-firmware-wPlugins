#include "../marauder_gui_app_i.h"

/* WiFi half of Marauder's cross-hardware "Recon" mission mode (firmware v1.16.0). Recon's own
   rich dashboard (churn graph, per-channel activity, live association feed) only exists on
   Marauder's own TFT screen (ReconMission::drawDashboard() is entirely #ifdef HAS_SCREEN) and its
   session manifest only exists on Marauder's own SD card (also unavailable on our GPIO devboard)
   - over serial, "recon status" only ever prints a single "active"/"stopped" word, nothing else.

   But "recon wifi" starts the exact same WIFI_SCAN_AP_STA scan scanall does, so the access point
   list it discovers is readable the exact same way scanall's is: "list -a", the same "[N][CH:x]
   ssid rssi" lines wifi_scanning.c already parses. So instead of a raw status readout, this reuses
   the same live-list dashboard every other passive detector in this app already uses (see
   bt_detect_general.c for the simplest example of the same pattern) - a real, growing list of
   discovered APs instead of one word. */
void marauder_gui_scene_wifi_recon_on_enter(void* context) {
    MarauderGuiApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);
    marauder_gui_list_scan_start(
        app,
        "recon wifi",
        "list -a",
        marauder_gui_text(app, "WiFi Kesif Calisiyor..", "WiFi Recon Running.."),
        marauder_gui_text(app, "AP bulunamadi...", "No AP found..."));
}

bool marauder_gui_scene_wifi_recon_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_list_scan_handle_back(context, event);
}

void marauder_gui_scene_wifi_recon_on_exit(void* context) {
    marauder_gui_list_scan_exit(context);
}
