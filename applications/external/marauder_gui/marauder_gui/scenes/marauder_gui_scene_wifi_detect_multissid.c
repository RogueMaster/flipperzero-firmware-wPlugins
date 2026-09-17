#include "../marauder_gui_app_i.h"

void marauder_gui_scene_wifi_detect_multissid_on_enter(void* context) {
    MarauderGuiApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);
    marauder_gui_list_scan_start(
        app,
        "sniffmultissid",
        "list -m",
        marauder_gui_text(app, "MultiSSID Araniyor..", "Searching MultiSSID.."),
        marauder_gui_text(app, "MultiSSID bulunamadi...", "No MultiSSID found..."));
}

bool marauder_gui_scene_wifi_detect_multissid_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_list_scan_handle_back(context, event);
}

void marauder_gui_scene_wifi_detect_multissid_on_exit(void* context) {
    marauder_gui_list_scan_exit(context);
}
