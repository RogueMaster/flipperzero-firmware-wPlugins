#include "../marauder_gui_app_i.h"
#include <stdlib.h>
#include <string.h>

/* Same idea as wifi_scanning.c/wifi_probe_sniff.c: "list -i" only reads Marauder's internal IP
   list, it doesn't touch scan state, so we poll it periodically while "pingscan" keeps running
   to build the list live (populating IPs to pick a port-scan target from). */
#define WIFI_PORT_PICK_REFRESH_TICKS 15 /* ~1.5s at the app's 100ms tick period */
#define WIFI_PORT_PICK_MARQUEE_TICKS 3
#define WIFI_PORT_PICK_MARQUEE_DELAY_TICKS \
    30 /* ~3s pause before a highlighted row starts scrolling */

static void
    marauder_gui_scene_wifi_port_scan_pick_ip_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] != '[') return;

    long idx = strtol(line + 1, NULL, 10);
    if(idx < 0 || (size_t)idx != app->ap_count) return;
    if(app->ap_count >= MARAUDER_AP_LIST_MAX) return;

    strncpy(app->ap_list[app->ap_count], line, MARAUDER_LINE_MAX - 1);
    app->ap_list[app->ap_count][MARAUDER_LINE_MAX - 1] = '\0';
    app->ap_count++;

    marauder_gui_wifi_list_redraw(app);
}

static void marauder_gui_scene_wifi_port_scan_pick_ip_tick(MarauderGuiApp* app) {
    app->wifi_scan_refresh_tick++;
    if(app->wifi_scan_refresh_tick >= WIFI_PORT_PICK_REFRESH_TICKS) {
        app->wifi_scan_refresh_tick = 0;
        if(!app->wifi_scan_frozen) {
            marauder_uart_send_line(app->uart, "list -i");
        }
    }

    if(app->wifi_list_marquee_delay > 0) {
        app->wifi_list_marquee_delay--;
    } else {
        app->wifi_list_marquee_hold++;
        if(app->wifi_list_marquee_hold >= WIFI_PORT_PICK_MARQUEE_TICKS) {
            app->wifi_list_marquee_hold = 0;
            app->wifi_list_marquee_tick++;
            marauder_gui_wifi_list_redraw(app);
        }
    }
}

void marauder_gui_scene_wifi_port_scan_pick_ip_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->ap_count = 0;
    app->wifi_scan_refresh_tick = 0;
    app->wifi_scan_frozen = false;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = WIFI_PORT_PICK_MARQUEE_DELAY_TICKS;
    app->wifi_list_scanning_label = marauder_gui_text(app, "IP Araniyor..", "Searching IP..");
    /* pingscan has to wait out an ICMP timeout per unreachable host, so the first hits can take
       a while - word this as "still searching", not "not found", or it reads as an immediate
       failure the instant the screen opens (ap_count is 0 until the first reply arrives). */
    app->wifi_list_empty_label = marauder_gui_text(
        app, "IP araniyor... (biraz surebilir)", "Searching IP... (may take a while)");

    app->uart_line_handler = marauder_gui_scene_wifi_port_scan_pick_ip_uart_line;
    app->tick_handler = marauder_gui_scene_wifi_port_scan_pick_ip_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);

    marauder_uart_send_line(app->uart, "pingscan");
}

bool marauder_gui_scene_wifi_port_scan_pick_ip_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->ap_count) {
            app->selected_ip_index = (int)event.event;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiPortScanStatus);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(!app->wifi_scan_frozen) {
            app->wifi_scan_frozen = true;
            /* Deliberately NOT sending "stopscan" here - this scene's "pingscan" runs while
               already STA-joined to a real network, and stopscan's shutdownWiFi() permanently
               kills the WiFi driver (until reboot) if Marauder's wifi_connected flag happens to
               read false at that instant. Same bug class as wifi_join_status.c; see
               marauder_gui_log_scan.c's "_keep_running" helpers for the fuller explanation. */
            marauder_gui_wifi_list_redraw(app);
        } else {
            scene_manager_previous_scene(app->scene_manager);
        }
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_port_scan_pick_ip_on_exit(void* context) {
    MarauderGuiApp* app = context;

    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
}
