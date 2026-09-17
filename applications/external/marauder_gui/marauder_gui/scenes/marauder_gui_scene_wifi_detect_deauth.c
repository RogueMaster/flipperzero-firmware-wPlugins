#include "../marauder_gui_app_i.h"

/* One line per detected frame: "<rssi> Ch: <n> <src_mac> -> <dst_mac>" (see WiFiScan.cpp's
   beaconSnifferCallback for WIFI_SCAN_DEAUTH) - no list -X flag, so just log the raw stream. */
static void marauder_gui_scene_wifi_detect_deauth_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    marauder_gui_log_scan_append(app, line);
    marauder_gui_log_scan_redraw(app, "Deauth Sniff");
}

static void marauder_gui_scene_wifi_detect_deauth_start(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, "sniffdeauth");
    marauder_gui_log_scan_redraw(app, "Deauth Sniff");
}

void marauder_gui_scene_wifi_detect_deauth_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_wifi_detect_deauth_uart_line;
    app->resume_handler = marauder_gui_scene_wifi_detect_deauth_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_wifi_detect_deauth_start(app);
}

bool marauder_gui_scene_wifi_detect_deauth_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back(context, event);
}

void marauder_gui_scene_wifi_detect_deauth_on_exit(void* context) {
    marauder_gui_log_scan_exit(context);
}
