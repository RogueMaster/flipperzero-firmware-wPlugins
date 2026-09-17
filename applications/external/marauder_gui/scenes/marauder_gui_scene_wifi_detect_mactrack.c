#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* mactrack pushes a full top-10 "who's been following me" table once a second, each refresh
   preceded by a "---------------" separator line, each entry printed as
   "<mac> Frames: <n> Last Seen: <s>s" (see WiFiScan.cpp's updateTrackerUI() - the on-device
   screen additionally colors/prefixes a "FOLLOWING" tag we don't get over serial, so the ranking
   itself, by descending frame count, is the closest signal we have to "this one's suspicious").
   Originally dumped as a raw scrolling terminal-style log (the same lines with their exact
   "Frames:"/"Last Seen:" wording) - reusing the indexed WifiList view instead turns each entry
   into its own row (marquee-scrollable when selected, like a long AP SSID) rather than a wall of
   repeating text, and the existing freeze/resume convention applies for free. */
#define MACTRACK_MARQUEE_TICKS       3
#define MACTRACK_MARQUEE_DELAY_TICKS 30 /* ~3s pause before a highlighted row starts scrolling */

static void
    marauder_gui_scene_wifi_detect_mactrack_uart_line(MarauderGuiApp* app, const char* line) {
    if(strcmp(line, "---------------") == 0) {
        /* Start of a fresh table - the ranking can reorder entirely between refreshes (sorted
           by frame count, which keeps climbing), so this can't be append-only-dedup by index
           the way the AP list is; a full rebuild is the only correct option while live. */
        app->ap_count = 0;
        app->wifi_list_selected = 0;
        app->wifi_list_scroll_offset = 0;
        marauder_gui_wifi_list_redraw(app);
        return;
    }
    if(line[0] == '\0') return;
    if(app->ap_count >= MARAUDER_AP_LIST_MAX) return;

    const char* frames_marker = strstr(line, " Frames: ");
    const char* seen_marker = strstr(line, " Last Seen: ");
    if(!frames_marker || !seen_marker) return;

    char mac[20];
    size_t mac_len = (size_t)(frames_marker - line);
    if(mac_len >= sizeof(mac)) mac_len = sizeof(mac) - 1;
    memcpy(mac, line, mac_len);
    mac[mac_len] = '\0';

    long frames = strtol(frames_marker + 9, NULL, 10);
    long seconds = strtol(seen_marker + 12, NULL, 10);

    snprintf(
        app->ap_list[app->ap_count],
        sizeof(app->ap_list[app->ap_count]),
        "%s  F:%ld  %lds",
        mac,
        frames,
        seconds);
    app->ap_count++;

    marauder_gui_wifi_list_redraw(app);
}

static void marauder_gui_scene_wifi_detect_mactrack_tick(MarauderGuiApp* app) {
    if(app->wifi_list_marquee_delay > 0) {
        app->wifi_list_marquee_delay--;
    } else {
        app->wifi_list_marquee_hold++;
        if(app->wifi_list_marquee_hold >= MACTRACK_MARQUEE_TICKS) {
            app->wifi_list_marquee_hold = 0;
            app->wifi_list_marquee_tick++;
            marauder_gui_wifi_list_redraw(app);
        }
    }
}

void marauder_gui_scene_wifi_detect_mactrack_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->ap_count = 0;
    app->wifi_scan_frozen = false;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = MACTRACK_MARQUEE_DELAY_TICKS;
    app->wifi_list_scanning_label = marauder_gui_text(app, "MAC Monitor", "MAC Monitor");
    app->wifi_list_empty_label =
        marauder_gui_text(app, "Cihaz araniyor...", "Searching for devices...");

    app->uart_line_handler = marauder_gui_scene_wifi_detect_mactrack_uart_line;
    app->tick_handler = marauder_gui_scene_wifi_detect_mactrack_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);

    marauder_uart_send_line(app->uart, "mactrack");
}

bool marauder_gui_scene_wifi_detect_mactrack_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        /* No per-row action (read-only table) - Ok only means something while frozen: resume,
           same idea as the log-scan "Devam Et" button, just handled locally since the WifiList
           view already fires Ok as a custom event on its own (no button-widget trick needed). */
        if(app->wifi_scan_frozen) {
            app->wifi_scan_frozen = false;
            app->ap_count = 0;
            app->wifi_list_selected = 0;
            app->wifi_list_scroll_offset = 0;
            marauder_uart_send_line(app->uart, "mactrack");
            marauder_gui_wifi_list_redraw(app);
        }
        consumed = true;
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

void marauder_gui_scene_wifi_detect_mactrack_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
}
