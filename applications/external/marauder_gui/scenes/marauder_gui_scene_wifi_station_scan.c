#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIFI_STATION_SCAN_REFRESH_TICKS 15 /* ~1.5s at the app's 100ms tick period */
#define WIFI_STATION_MARQUEE_TICKS      3
#define WIFI_STATION_MARQUEE_DELAY_TICKS \
    30 /* ~3s pause before a highlighted row starts scrolling */

static bool
    marauder_gui_scene_wifi_station_scan_have_index(MarauderGuiApp* app, long global_index) {
    for(size_t i = 0; i < app->ap_count; i++) {
        if(app->station_global_index[i] == global_index) return true;
    }
    return false;
}

/* "list -c" dumps every AP with its stations nested under it as indented "  [i] mac" lines
   (see CommandLine.cpp's LIST_AP_CMD "-c" branch) - i is the GLOBAL index into Marauder's
   stations list, not sequential per AP, so we track whether we're currently inside our
   selected AP's block as top-level "[N]..." lines stream past, keeping only indented lines
   seen while inside it. Re-sent full dumps are deduped by that global index rather than by
   position, since a station's index isn't necessarily its row number here. */
static void marauder_gui_scene_wifi_station_scan_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '[') {
        long idx = strtol(line + 1, NULL, 10);
        app->wifi_station_capturing = (idx == app->selected_ap_index);
        return;
    }

    if(!app->wifi_station_capturing) return;

    const char* p = line;
    while(*p == ' ')
        p++;
    if(*p != '[') return;

    long sidx = strtol(p + 1, NULL, 10);
    if(sidx < 0) return;
    if(marauder_gui_scene_wifi_station_scan_have_index(app, sidx)) return;
    if(app->ap_count >= MARAUDER_AP_LIST_MAX) return;

    strncpy(app->ap_list[app->ap_count], p, MARAUDER_LINE_MAX - 1);
    app->ap_list[app->ap_count][MARAUDER_LINE_MAX - 1] = '\0';
    app->station_global_index[app->ap_count] = (int)sidx;
    app->ap_count++;

    marauder_gui_wifi_list_redraw(app);
}

static void marauder_gui_scene_wifi_station_scan_tick(MarauderGuiApp* app) {
    app->wifi_scan_refresh_tick++;
    if(app->wifi_scan_refresh_tick >= WIFI_STATION_SCAN_REFRESH_TICKS) {
        app->wifi_scan_refresh_tick = 0;
        if(!app->wifi_scan_frozen) {
            marauder_uart_send_line(app->uart, "list -c");
        }
    }

    if(app->wifi_list_marquee_delay > 0) {
        app->wifi_list_marquee_delay--;
    } else {
        app->wifi_list_marquee_hold++;
        if(app->wifi_list_marquee_hold >= WIFI_STATION_MARQUEE_TICKS) {
            app->wifi_list_marquee_hold = 0;
            app->wifi_list_marquee_tick++;
            marauder_gui_wifi_list_redraw(app);
        }
    }
}

void marauder_gui_scene_wifi_station_scan_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->ap_count = 0;
    app->wifi_scan_refresh_tick = 0;
    app->wifi_scan_frozen = false;
    app->wifi_station_capturing = false;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = WIFI_STATION_MARQUEE_DELAY_TICKS;
    app->wifi_list_scanning_label =
        marauder_gui_text(app, "Istemci Araniyor..", "Searching Client..");
    app->wifi_list_empty_label =
        marauder_gui_text(app, "Istemci bulunamadi...", "No client found...");

    app->uart_line_handler = marauder_gui_scene_wifi_station_scan_uart_line;
    app->tick_handler = marauder_gui_scene_wifi_station_scan_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);

    /* filterActive() (checked by the deauth -c handler) tracks AP selection, so the AP must be
       marked selected here even though the actual attack command fires from the next scene. */
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
    marauder_uart_send_line(app->uart, cmd);
    marauder_uart_send_line(app->uart, "list -c");
}

bool marauder_gui_scene_wifi_station_scan_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->ap_count) {
            app->selected_station_index = app->station_global_index[event.event];
            strncpy(app->selected_station_label, app->ap_list[event.event], MARAUDER_LINE_MAX - 1);
            app->selected_station_label[MARAUDER_LINE_MAX - 1] = '\0';
            if(app->wifi_ap_attack_type == 11) {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiCloneStaMac);
            } else if(app->wifi_ap_attack_type == 15) {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiStationFoxHunt);
            } else {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiAttack);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(!app->wifi_scan_frozen) {
            app->wifi_scan_frozen = true;
            marauder_uart_send_line(app->uart, "stopscan");
            marauder_gui_wifi_list_redraw(app);
        } else {
            scene_manager_previous_scene(app->scene_manager);
        }
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_station_scan_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
}
