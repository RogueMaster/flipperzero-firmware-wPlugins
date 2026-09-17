#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* Pwnagotchi beacons print as two separate lines per detection - "Name: <name>" then
   "Pwnd #: <count>" (see WiFiScan.cpp's processPwnagotchiBeacon(), no list -X flag covers
   this) - so we hold the name until its matching count line arrives, then log one combined
   entry. */
static void marauder_gui_scene_wifi_detect_pwnagotchi_uart_line(MarauderGuiApp* app, const char* line) {
    if(strncmp(line, "Name: ", 6) == 0) {
        strncpy(app->pending_pwn_name, line + 6, sizeof(app->pending_pwn_name) - 1);
        app->pending_pwn_name[sizeof(app->pending_pwn_name) - 1] = '\0';
    } else if(strncmp(line, "Pwnd #: ", 8) == 0) {
        char entry[64];
        snprintf(entry, sizeof(entry), "%s (Pwnd: %s)", app->pending_pwn_name, line + 8);
        marauder_gui_log_scan_append(app, entry);
        marauder_gui_log_scan_redraw(app, "Pwnagotchi");
    }
}

static void marauder_gui_scene_wifi_detect_pwnagotchi_start(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, "sniffpwn");
    marauder_gui_log_scan_redraw(app, "Pwnagotchi");
}

void marauder_gui_scene_wifi_detect_pwnagotchi_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->pending_pwn_name[0] = '\0';
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_wifi_detect_pwnagotchi_uart_line;
    app->resume_handler = marauder_gui_scene_wifi_detect_pwnagotchi_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_wifi_detect_pwnagotchi_start(app);
}

bool marauder_gui_scene_wifi_detect_pwnagotchi_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back(context, event);
}

void marauder_gui_scene_wifi_detect_pwnagotchi_on_exit(void* context) {
    marauder_gui_log_scan_exit(context);
}
