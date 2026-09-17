#include "../marauder_gui_app_i.h"
#include <stdio.h>

/* "foxhunt -w N" (WIFI_SCAN_SIG_STREN) reads the AP straight out of access_points->get(N) at
   parse time (setFoxHuntTarget()) - no ".selected" side effect to undo on exit, unlike
   packetcount/attacks. Its RSSI readout only prints over Serial from the WiFi packet-sniffer
   callback ("<ssid> RSSI: <n>" per matching frame, see WiFiScan.cpp's WIFI_SCAN_SIG_STREN
   branch) - this is the one Fox Hunt variant that actually has a usable serial output on a
   screenless board; the Bluetooth variants only ever draw their live RSSI on the touchscreen
   (runFoxHunt() is entirely inside "#ifdef HAS_SCREEN"), so they were left out. */
static void marauder_gui_scene_wifi_fox_hunt_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    marauder_gui_log_scan_append(app, line);
    marauder_gui_log_scan_redraw(app, "Fox Hunt");
}

static void marauder_gui_scene_wifi_fox_hunt_start(MarauderGuiApp* app) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "foxhunt -w %d", app->selected_ap_index);
    marauder_uart_send_line(app->uart, cmd);
    marauder_gui_log_scan_redraw(app, "Fox Hunt");
}

void marauder_gui_scene_wifi_fox_hunt_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_wifi_fox_hunt_uart_line;
    app->resume_handler = marauder_gui_scene_wifi_fox_hunt_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_wifi_fox_hunt_start(app);
}

bool marauder_gui_scene_wifi_fox_hunt_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back(context, event);
}

void marauder_gui_scene_wifi_fox_hunt_on_exit(void* context) {
    marauder_gui_log_scan_exit(context);
}
