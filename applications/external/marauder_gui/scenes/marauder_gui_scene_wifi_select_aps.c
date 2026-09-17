#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Standalone multi-AP selection screen, mirroring the original touchscreen GUI's "Select APs"
   (a checkbox list toggling each AP's `.selected` flag via "select -a <idx>", plus its own
   "Select ALL" - not built here, see roadmap). Unlike every other AP-list scene in this app, Ok
   does NOT navigate away - it just flips that one row's mark and re-sends "select -a <idx>"
   (Marauder toggles, it doesn't set, so re-sending is exactly "press it again").

   Right freezes the scan (if not already frozen) and moves to
   MarauderGuiSceneWifiSelectApsAttackMenu to pick which attack to fire at every AP left checked
   here. This works with zero extra plumbing on the Marauder side: reading WiFiScan.cpp confirms
   attack -t deauth/probe/beacon -a/sae/csa/quiet/badmsg/sleep (every type except the
   single-station-targeted deauth) already loop over EVERY access_point with .selected == true
   and hop each one's own channel internally (see sendDeauthFrame/sendProbeAttack/
   broadcastCustomBeacon) - so leaving several APs checked here and firing one of those from the
   next menu already attacks all of them, no per-AP looping needed on our end. */

#define WIFI_SELECT_APS_REFRESH_TICKS       15 /* ~1.5s at the app's 100ms tick period */
#define WIFI_SELECT_APS_MARQUEE_TICKS       3
#define WIFI_SELECT_APS_MARQUEE_DELAY_TICKS 30

static void marauder_gui_scene_wifi_select_aps_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] != '[') return;

    long idx = strtol(line + 1, NULL, 10);
    if(idx < 0 || (size_t)idx != app->ap_count) return;
    if(app->ap_count >= MARAUDER_AP_LIST_MAX) return;

    /* Reserve the first 4 chars of the stored line for our own "[ ] "/"[x] " checkbox marker -
       Marauder's own line always starts with its own "[N]..." index, which stays right after
       the marker, so the row still reads naturally once printed. */
    char* dst = app->ap_list[app->ap_count];
    dst[0] = '[';
    dst[1] = ' ';
    dst[2] = ']';
    dst[3] = ' ';
    strncpy(dst + 4, line, MARAUDER_LINE_MAX - 5);
    dst[MARAUDER_LINE_MAX - 1] = '\0';
    app->ap_count++;

    marauder_gui_wifi_list_redraw(app);
}

static void marauder_gui_scene_wifi_select_aps_tick(MarauderGuiApp* app) {
    app->wifi_scan_refresh_tick++;
    if(app->wifi_scan_refresh_tick >= WIFI_SELECT_APS_REFRESH_TICKS) {
        app->wifi_scan_refresh_tick = 0;
        if(!app->wifi_scan_frozen) {
            marauder_uart_send_line(app->uart, "list -a");
        }
    }

    if(app->wifi_list_marquee_delay > 0) {
        app->wifi_list_marquee_delay--;
    } else {
        app->wifi_list_marquee_hold++;
        if(app->wifi_list_marquee_hold >= WIFI_SELECT_APS_MARQUEE_TICKS) {
            app->wifi_list_marquee_hold = 0;
            app->wifi_list_marquee_tick++;
            marauder_gui_wifi_list_redraw(app);
        }
    }
}

void marauder_gui_scene_wifi_select_aps_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->ap_count = 0;
    app->wifi_scan_refresh_tick = 0;
    app->wifi_scan_frozen = false;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = WIFI_SELECT_APS_MARQUEE_DELAY_TICKS;
    /* Kept short so it doesn't run under the selected-count badge drawn top-right (see
       marauder_gui_wifi_list_draw_selected_badge in this shared view's file). */
    app->wifi_list_scanning_label = marauder_gui_text(app, "AP Araniyor..", "Searching AP..");
    app->wifi_list_empty_label = marauder_gui_text(app, "AP araniyor...", "Searching for AP...");
    app->wifi_list_show_selected_count = true;
    app->wifi_list_frozen_label =
        marauder_gui_text(app, "Durdu (Sag:Saldir)", "Stopped (Right:Attack)");

    app->uart_line_handler = marauder_gui_scene_wifi_select_aps_uart_line;
    app->tick_handler = marauder_gui_scene_wifi_select_aps_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);

    marauder_uart_send_line(app->uart, "scanall");
}

bool marauder_gui_scene_wifi_select_aps_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MARAUDER_WIFI_LIST_PROCEED_CUSTOM_EVENT) {
            size_t selected = 0;
            for(size_t i = 0; i < app->ap_count; i++) {
                if(app->ap_list[i][1] == 'x') selected++;
            }
            if(selected > 0) {
                if(!app->wifi_scan_frozen) {
                    app->wifi_scan_frozen = true;
                    marauder_uart_send_line(app->uart, "stopscan");
                    marauder_gui_wifi_list_redraw(app);
                }
                scene_manager_next_scene(
                    app->scene_manager, MarauderGuiSceneWifiSelectApsAttackMenu);
            }
            consumed = true;
        } else if(event.event < app->ap_count) {
            char* row = app->ap_list[event.event];
            row[1] = (row[1] == 'x') ? ' ' : 'x';

            char cmd[32];
            snprintf(cmd, sizeof(cmd), "select -a %u", (unsigned)event.event);
            marauder_uart_send_line(app->uart, cmd);

            marauder_gui_wifi_list_redraw(app);
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

void marauder_gui_scene_wifi_select_aps_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    app->wifi_list_show_selected_count = false;
    app->wifi_list_frozen_label = NULL;
    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
}
