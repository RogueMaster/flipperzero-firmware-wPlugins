#include "../marauder_gui_app_i.h"
#include <string.h>

/* SSID filter picker for the .pcap table viewer - a checkbox list of the SSIDs actually seen in
   the currently-open file (from app->pcap_index), not a free-typed field. Same "[ ]"/"[x]"
   marker-in-string convention as wifi_select_aps.c: toggling a row here directly edits
   app->pcap_filter_ssids/pcap_filter_ssid_count, which pcap_view.c re-applies live, so there is
   nothing to "confirm" - Back just returns to pcap_filter.c. */

static bool marauder_gui_scene_pcap_filter_ssid_is_checked(MarauderGuiApp* app, const char* ssid) {
    for(size_t i = 0; i < app->pcap_filter_ssid_count; i++) {
        if(strcmp(app->pcap_filter_ssids[i], ssid) == 0) return true;
    }
    return false;
}

static void marauder_gui_scene_pcap_filter_ssid_toggle(MarauderGuiApp* app, const char* ssid) {
    for(size_t i = 0; i < app->pcap_filter_ssid_count; i++) {
        if(strcmp(app->pcap_filter_ssids[i], ssid) == 0) {
            /* Remove by shifting the tail down - the list is tiny (<= MARAUDER_AP_LIST_MAX). */
            for(size_t j = i + 1; j < app->pcap_filter_ssid_count; j++) {
                strncpy(
                    app->pcap_filter_ssids[j - 1],
                    app->pcap_filter_ssids[j],
                    sizeof(app->pcap_filter_ssids[0]));
            }
            app->pcap_filter_ssid_count--;
            return;
        }
    }
    if(app->pcap_filter_ssid_count < MARAUDER_PCAP_SSID_FILTER_MAX) {
        strncpy(
            app->pcap_filter_ssids[app->pcap_filter_ssid_count],
            ssid,
            sizeof(app->pcap_filter_ssids[0]) - 1);
        app->pcap_filter_ssids[app->pcap_filter_ssid_count][sizeof(app->pcap_filter_ssids[0]) - 1] =
            '\0';
        app->pcap_filter_ssid_count++;
    }
}

void marauder_gui_scene_pcap_filter_ssid_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->ap_count = 0;
    for(size_t i = 0; i < app->pcap_index_count && app->ap_count < MARAUDER_AP_LIST_MAX; i++) {
        const char* ssid = app->pcap_index[i].ssid;
        if(!ssid[0]) continue;

        bool already_listed = false;
        for(size_t j = 0; j < app->ap_count; j++) {
            if(strcmp(app->ap_list[j] + 4, ssid) == 0) {
                already_listed = true;
                break;
            }
        }
        if(already_listed) continue;

        bool checked = marauder_gui_scene_pcap_filter_ssid_is_checked(app, ssid);
        app->ap_list[app->ap_count][0] = '[';
        app->ap_list[app->ap_count][1] = checked ? 'x' : ' ';
        app->ap_list[app->ap_count][2] = ']';
        app->ap_list[app->ap_count][3] = ' ';
        strncpy(app->ap_list[app->ap_count] + 4, ssid, MARAUDER_LINE_MAX - 5);
        app->ap_list[app->ap_count][MARAUDER_LINE_MAX - 1] = '\0';
        app->ap_count++;
    }

    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = 0;
    app->wifi_scan_frozen = true;
    app->wifi_list_show_selected_count = false;
    app->wifi_list_frozen_label = marauder_gui_text(app, "Gorulen SSID'ler", "Seen SSIDs");
    app->wifi_list_empty_label =
        marauder_gui_text(app, "Bu dosyada SSID yok", "No SSIDs in this file");

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);
    marauder_gui_wifi_list_redraw(app);
}

bool marauder_gui_scene_pcap_filter_ssid_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->ap_count) {
            char* row = app->ap_list[event.event];
            marauder_gui_scene_pcap_filter_ssid_toggle(app, row + 4);
            row[1] = (row[1] == 'x') ? ' ' : 'x';
            marauder_gui_wifi_list_redraw(app);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_pcap_filter_ssid_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->wifi_list_frozen_label = NULL;
    app->tick_handler = NULL;
}
