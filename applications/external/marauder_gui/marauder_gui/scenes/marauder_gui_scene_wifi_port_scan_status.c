#include "../marauder_gui_app_i.h"
#include <stdio.h>

/* "portscan -t <ip index> -a" checks every port on the target IP (see WiFiScan.cpp's
   portScan()), printing a "Checking IP: x Port: y" progress line every 1000 ports plus an
   "ip: port" line whenever an open one is found - raw lines, log-scan pattern. This can take a
   very long time (every port, 100ms timeout each) - the user is expected to just Back out once
   they've seen enough. */
static void marauder_gui_scene_wifi_port_scan_status_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    marauder_gui_log_scan_append(app, line);
    marauder_gui_log_scan_redraw(app, "Port Scan");
}

static void marauder_gui_scene_wifi_port_scan_status_start(MarauderGuiApp* app) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "portscan -t %d -a", app->selected_ip_index);
    marauder_uart_send_line(app->uart, cmd);
    marauder_gui_log_scan_redraw(app, "Port Scan");
}

void marauder_gui_scene_wifi_port_scan_status_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_wifi_port_scan_status_uart_line;
    app->resume_handler = marauder_gui_scene_wifi_port_scan_status_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_wifi_port_scan_status_start(app);
}

bool marauder_gui_scene_wifi_port_scan_status_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back_keep_running(context, event);
}

void marauder_gui_scene_wifi_port_scan_status_on_exit(void* context) {
    marauder_gui_log_scan_exit_keep_running(context);
}
