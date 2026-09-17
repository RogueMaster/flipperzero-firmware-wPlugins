#include "../marauder_gui_app_i.h"

/* One raw IP address per line found (see WiFiScan.cpp's fullARP() - just
   Serial.println(ip.toString())), no list -X flag - log-scan pattern like the detectors. */
static void marauder_gui_scene_wifi_arp_scan_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    marauder_gui_log_scan_append(app, line);
    marauder_gui_log_scan_redraw(app, "ARP Scan");
}

static void marauder_gui_scene_wifi_arp_scan_start(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, "arpscan");
    marauder_gui_log_scan_redraw(app, "ARP Scan");
}

void marauder_gui_scene_wifi_arp_scan_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_wifi_arp_scan_uart_line;
    app->resume_handler = marauder_gui_scene_wifi_arp_scan_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_wifi_arp_scan_start(app);
}

bool marauder_gui_scene_wifi_arp_scan_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back_keep_running(context, event);
}

void marauder_gui_scene_wifi_arp_scan_on_exit(void* context) {
    marauder_gui_log_scan_exit_keep_running(context);
}
