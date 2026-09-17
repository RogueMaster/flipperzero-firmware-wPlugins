#include "../marauder_gui_app_i.h"
#include <stdlib.h>
#include <string.h>

/* Same idea as wifi_scanning.c: "list -t" only reads Marauder's internal airtags list, it
   doesn't touch scan state, so we poll it periodically while "sniffbt -t airtag" keeps running.
   Reuses the app's shared list-view state (ap_list/ap_count/wifi_list_*) and the WifiList
   custom View - see marauder_gui_scene_wifi_scanning.c for that view's implementation. */
#define BT_TRACKER_SCAN_REFRESH_TICKS  15 /* ~1.5s at the app's 100ms tick period */
#define BT_TRACKER_MARQUEE_TICKS       3
#define BT_TRACKER_MARQUEE_DELAY_TICKS 30 /* ~3s pause before a highlighted row starts scrolling */

/* Marauder prints tracker entries as "[<index>]MAC: <mac>" (see CommandLine.cpp's "list -t"
   handler), in the same order as its internal airtags list (the index "findmy -t"/"spoofat -t"
   expect). Every periodic "list -t" re-sends the full list, so only append an entry the first
   time its index is seen, same fix as the AP list. */
static void marauder_gui_scene_bt_tracker_scan_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] != '[') return;

    long idx = strtol(line + 1, NULL, 10);
    if(idx < 0 || (size_t)idx != app->ap_count) return;
    if(app->ap_count >= MARAUDER_AP_LIST_MAX) return;

    strncpy(app->ap_list[app->ap_count], line, MARAUDER_LINE_MAX - 1);
    app->ap_list[app->ap_count][MARAUDER_LINE_MAX - 1] = '\0';
    app->ap_count++;

    marauder_gui_wifi_list_redraw(app);
}

static void marauder_gui_scene_bt_tracker_scan_tick(MarauderGuiApp* app) {
    app->wifi_scan_refresh_tick++;
    if(app->wifi_scan_refresh_tick >= BT_TRACKER_SCAN_REFRESH_TICKS) {
        app->wifi_scan_refresh_tick = 0;
        if(!app->wifi_scan_frozen) {
            marauder_uart_send_line(app->uart, "list -t");
        }
    }

    if(app->wifi_list_marquee_delay > 0) {
        app->wifi_list_marquee_delay--;
    } else {
        app->wifi_list_marquee_hold++;
        if(app->wifi_list_marquee_hold >= BT_TRACKER_MARQUEE_TICKS) {
            app->wifi_list_marquee_hold = 0;
            app->wifi_list_marquee_tick++;
            marauder_gui_wifi_list_redraw(app);
        }
    }
}

void marauder_gui_scene_bt_tracker_scan_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->ap_count = 0;
    app->wifi_scan_refresh_tick = 0;
    app->wifi_scan_frozen = false;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = BT_TRACKER_MARQUEE_DELAY_TICKS;
    app->wifi_list_scanning_label = marauder_gui_text(app, "Araniyor...", "Searching...");
    app->wifi_list_empty_label =
        marauder_gui_text(app, "AirTag/tracker araniyor...", "Searching AirTag/tracker...");

    app->uart_line_handler = marauder_gui_scene_bt_tracker_scan_uart_line;
    app->tick_handler = marauder_gui_scene_bt_tracker_scan_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);

    marauder_uart_send_line(app->uart, "sniffbt -t airtag");
}

bool marauder_gui_scene_bt_tracker_scan_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->ap_count) {
            app->selected_tracker_index = (int)event.event;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtTrackerAction);
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

void marauder_gui_scene_bt_tracker_scan_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
}
