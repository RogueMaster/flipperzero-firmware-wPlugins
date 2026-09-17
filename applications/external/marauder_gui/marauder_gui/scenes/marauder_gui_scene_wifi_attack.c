#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* AP-targeted attacks that reuse this same select-then-attack flow. "AP Tara / Deauth" (type 0)
   is the original entry point; every other type here also turns out to need a selected AP -
   CommandLine.cpp only *checks* filterActive() (prints "You don't have any targets selected")
   for deauth/probe/beacon -a, but SAE/CSA/Quiet/BadMsg/AssocSleep's own loops in WiFiScan.cpp
   (saeAttackLoop, the WIFI_ATTACK_CSA/_QUIET dispatch, sendBadMsgAttack, sendAssocSleepAttack)
   all iterate `access_points` checking `.selected` with no fallback - with nothing selected
   they silently do nothing every tick. Confirmed by reading those loops after the user asked
   why no AP was being picked for these. Bad Msg/Assoc Sleep additionally walk that AP's known
   stations, which "scanall" (WIFI_SCAN_AP_STA) already populates as a side effect of the same
   scan the AP list came from - no extra step needed. */
static const struct {
    const char* title_tr;
    const char* title_en;
    const char* command;
} marauder_wifi_ap_attack_types[] = {
    {"Deauth Attack", "Deauth Attack", "attack -t deauth"},
    {"Probe Flood", "Probe Flood", "attack -t probe"},
    {"AP Klon Spam", "AP Clone Spam", "attack -t beacon -a"},
    {"Hedefli Deauth", "Targeted Deauth", "attack -t deauth -c"},
    {"SAE Commit Flood", "SAE Commit Flood", "attack -t sae"},
    {"Kanal Degistirme (CSA)", "Channel Switch (CSA)", "attack -t csa"},
    {"Quiet Time", "Quiet Time", "attack -t quiet"},
    {"Bad Msg", "Bad Msg", "attack -t badmsg"},
    {"Assoc Sleep", "Assoc Sleep", "attack -t sleep"},
};

/* AP lines look like "[N][CH:6] ssid rssi" - "select -a" does not follow the AP's channel by
   itself, so we have to set it explicitly or the attack silently fires on the wrong channel. */
static int marauder_gui_scene_wifi_attack_parse_channel(const char* line) {
    const char* p = strstr(line, "CH:");
    if(!p) return -1;
    return (int)strtol(p + 3, NULL, 10);
}

static void marauder_gui_scene_wifi_attack_tick(MarauderGuiApp* app) {
    app->attack_spectrum_phase++;
    marauder_gui_attack_view_redraw(app);
}

void marauder_gui_scene_wifi_attack_on_enter(void* context) {
    MarauderGuiApp* app = context;

    const char* target = "AP";
    if(app->wifi_attack_multi_ap) {
        size_t selected = 0;
        for(size_t i = 0; i < app->ap_count; i++) {
            if(app->ap_list[i][1] == 'x') selected++;
        }
        snprintf(
            app->attack_view_target_buf,
            sizeof(app->attack_view_target_buf),
            marauder_gui_text(app, "%u AP secili", "%u AP selected"),
            (unsigned)selected);
        target = app->attack_view_target_buf;
    } else if(app->wifi_ap_attack_type == 3) {
        target = app->selected_station_label;
    } else if(app->selected_ap_index >= 0 && (size_t)app->selected_ap_index < app->ap_count) {
        target = app->ap_list[app->selected_ap_index];
    }

    app->attack_view_title = marauder_gui_text(
        app,
        marauder_wifi_ap_attack_types[app->wifi_ap_attack_type].title_tr,
        marauder_wifi_ap_attack_types[app->wifi_ap_attack_type].title_en);
    app->attack_view_target = target;
    app->attack_view_style = MarauderAttackViewStyleSpectrum;
    app->attack_spectrum_phase = 0;
    app->tick_handler = marauder_gui_scene_wifi_attack_tick;

    marauder_gui_attack_view_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewAttackStatus);

    char cmd[32];

    if(app->wifi_attack_multi_ap) {
        /* Every targeted AP is already marked .selected from wifi_select_aps.c, and every
           attack type reachable from that menu loops all .selected access_points internally,
           hopping each one's own channel itself (confirmed in WiFiScan.cpp's sendDeauthFrame/
           sendProbeAttack/broadcastCustomBeacon/sendBadMsgAttack/sendAssocSleepAttack) - nothing
           to select or channel-switch here. */
    } else if(app->wifi_ap_attack_type == 3) {
        /* AP is already selected (wifi_station_scan.c did that when this screen's flow
           started) - only the station needs selecting here. */
        snprintf(cmd, sizeof(cmd), "select -c %d", app->selected_station_index);
        marauder_uart_send_line(app->uart, cmd);
    } else {
        int channel = marauder_gui_scene_wifi_attack_parse_channel(target);
        if(channel > 0) {
            snprintf(cmd, sizeof(cmd), "channel -s %d", channel);
            marauder_uart_send_line(app->uart, cmd);
        }

        snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
        marauder_uart_send_line(app->uart, cmd);
    }

    marauder_uart_send_line(app->uart, marauder_wifi_ap_attack_types[app->wifi_ap_attack_type].command);

    app->attack_active = true;
}

bool marauder_gui_scene_wifi_attack_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom &&
        event.event == MARAUDER_ATTACK_STOP_CUSTOM_EVENT) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_attack_on_exit(void* context) {
    MarauderGuiApp* app = context;

    if(app->attack_active) {
        marauder_uart_send_line(app->uart, "stopscan");

        /* "select" toggles selection state, so toggle the same AP/station back off to leave a
           clean slate for the next attack - except in multi-AP mode, where the whole point of
           wifi_select_aps.c is to hold that selection for reuse (another attack, or a manual
           Terminal command), so we leave it untouched here. */
        if(!app->wifi_attack_multi_ap) {
            char cmd[32];
            if(app->wifi_ap_attack_type == 3) {
                snprintf(cmd, sizeof(cmd), "select -c %d", app->selected_station_index);
                marauder_uart_send_line(app->uart, cmd);
                snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
                marauder_uart_send_line(app->uart, cmd);
            } else {
                snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
                marauder_uart_send_line(app->uart, cmd);
            }
        }

        app->attack_active = false;
    }

    app->wifi_attack_multi_ap = false;
    app->tick_handler = NULL;
}
