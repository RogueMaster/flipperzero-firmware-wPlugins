#include "../marauder_gui_app_i.h"
#include <stdio.h>

/* "foxhunt -s <ap> <station>" (WIFI_SCAN_SIG_STREN) reads both indices straight out of
   access_points/stations at parse time - no ".selected" side effect of its own. But
   wifi_station_scan.c's on_enter unconditionally sends "select -a <ap>" for filterActive()'s
   sake (needed by Targeted Deauth, irrelevant here) before this scene ever runs, so that has to
   be undone on the way out - same fix as wifi_clone_sta_mac.c for the same reason. */
static void marauder_gui_scene_wifi_station_fox_hunt_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    marauder_gui_log_scan_append(app, line);
    marauder_gui_log_scan_redraw(app, marauder_gui_text(app, "Istemci Fox Hunt", "Client Fox Hunt"));
}

static void marauder_gui_scene_wifi_station_fox_hunt_start(MarauderGuiApp* app) {
    char cmd[48];
    snprintf(
        cmd, sizeof(cmd), "foxhunt -s %d %d", app->selected_ap_index, app->selected_station_index);
    marauder_uart_send_line(app->uart, cmd);
    marauder_gui_log_scan_redraw(app, marauder_gui_text(app, "Istemci Fox Hunt", "Client Fox Hunt"));
}

void marauder_gui_scene_wifi_station_fox_hunt_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_wifi_station_fox_hunt_uart_line;
    app->resume_handler = marauder_gui_scene_wifi_station_fox_hunt_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_wifi_station_fox_hunt_start(app);
}

bool marauder_gui_scene_wifi_station_fox_hunt_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back(context, event);
}

void marauder_gui_scene_wifi_station_fox_hunt_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_exit(app);

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
    marauder_uart_send_line(app->uart, cmd);
}
