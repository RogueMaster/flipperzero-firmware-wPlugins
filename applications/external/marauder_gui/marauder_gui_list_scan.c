#include "marauder_gui_app_i.h"
#include <stdlib.h>
#include <string.h>

/* Shared plumbing for "pure detector" scenes (Pineapple/MultiSSID/general-BLE/Flipper-finder/
   etc.): live list of "[N] ..." lines that Marauder keeps re-sending in full on every "list -X"
   poll, same append-only-new-index trick as wifi_scanning.c/bt_tracker_scan.c use, just without
   a follow-up action when an entry is picked (these are read-only detectors, not attack setup
   flows) - see marauder_gui_app_i.h for why this is a separate, more generic helper rather than
   reusing those scenes' own copies. */

#define LIST_SCAN_REFRESH_TICKS       15 /* ~1.5s at the app's 100ms tick period */
#define LIST_SCAN_MARQUEE_TICKS       3
#define LIST_SCAN_MARQUEE_DELAY_TICKS 30 /* ~3s pause before a highlighted row starts scrolling */

static void marauder_gui_list_scan_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] != '[') return;

    long idx = strtol(line + 1, NULL, 10);
    if(idx < 0 || (size_t)idx != app->ap_count) return;
    if(app->ap_count >= MARAUDER_AP_LIST_MAX) return;

    strncpy(app->ap_list[app->ap_count], line, MARAUDER_LINE_MAX - 1);
    app->ap_list[app->ap_count][MARAUDER_LINE_MAX - 1] = '\0';
    app->ap_count++;

    marauder_gui_wifi_list_redraw(app);
}

static void marauder_gui_list_scan_tick(MarauderGuiApp* app) {
    app->wifi_scan_refresh_tick++;
    if(app->wifi_scan_refresh_tick >= LIST_SCAN_REFRESH_TICKS) {
        app->wifi_scan_refresh_tick = 0;
        if(!app->wifi_scan_frozen) {
            marauder_uart_send_line(app->uart, app->list_scan_refresh_command);
        }
    }

    if(app->wifi_list_marquee_delay > 0) {
        app->wifi_list_marquee_delay--;
    } else {
        app->wifi_list_marquee_hold++;
        if(app->wifi_list_marquee_hold >= LIST_SCAN_MARQUEE_TICKS) {
            app->wifi_list_marquee_hold = 0;
            app->wifi_list_marquee_tick++;
            marauder_gui_wifi_list_redraw(app);
        }
    }
}

/* All four scenes using this helper just resend one fixed literal command - no scene-specific
   resume function needed, unlike the log-scan helper's users (several of those set up a channel
   or a selection first, see e.g. wifi_fox_hunt.c/wifi_packet_count.c). */
static void marauder_gui_list_scan_resume(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, app->list_scan_start_command);
    marauder_gui_wifi_list_redraw(app);
}

void marauder_gui_list_scan_start(
    MarauderGuiApp* app,
    const char* start_command,
    const char* refresh_command,
    const char* scanning_label,
    const char* empty_label) {
    app->ap_count = 0;
    app->wifi_scan_refresh_tick = 0;
    app->wifi_scan_frozen = false;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = LIST_SCAN_MARQUEE_DELAY_TICKS;
    app->wifi_list_scanning_label = scanning_label;
    app->wifi_list_empty_label = empty_label;
    app->list_scan_refresh_command = refresh_command;
    app->list_scan_start_command = start_command;

    app->uart_line_handler = marauder_gui_list_scan_uart_line;
    app->tick_handler = marauder_gui_list_scan_tick;
    app->resume_handler = marauder_gui_list_scan_resume;

    marauder_uart_send_line(app->uart, start_command);
}

bool marauder_gui_list_scan_handle_back(MarauderGuiApp* app, SceneManagerEvent event) {
    /* Ok while frozen: resume - re-issue the original start command and un-freeze, rather than
       forcing the user to back all the way out and re-enter just to keep watching. Only
       meaningful while frozen (there's no per-item action here to conflict with - see this
       file's top comment - so a stray Ok while still actively scanning is simply ignored). */
    if(event.type == SceneManagerEventTypeCustom) {
        if(!app->wifi_scan_frozen || !app->resume_handler) return false;
        app->wifi_scan_frozen = false;
        app->resume_handler(app);
        marauder_gui_wifi_list_redraw(app);
        return true;
    }

    if(event.type != SceneManagerEventTypeBack) return false;

    if(!app->wifi_scan_frozen) {
        /* First Back: stop and freeze the list in place so scrolling it doesn't fight the
           periodic rebuild (see wifi_scanning.c for the full story on this) */
        app->wifi_scan_frozen = true;
        marauder_uart_send_line(app->uart, "stopscan");
        marauder_gui_wifi_list_redraw(app);
    } else {
        /* Second Back: leave. Holding Back, at any point, is handled by the OS itself. */
        scene_manager_previous_scene(app->scene_manager);
    }

    return true;
}

void marauder_gui_list_scan_exit(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, "stopscan");
    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
    app->resume_handler = NULL;
}
