#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* "packetcount" (WIFI_SCAN_PACKET_RATE) only reports APs/stations that are marked `.selected`
   (see WiFiScan.cpp's renderPacketRate(), which silently skips every unselected entry) - so the
   picked AP has to actually be selected first, same as the SAE/CSA/etc. attacks. Also gated
   behind "!wifi_scan_obj.scanning()" like the MAC commands, which is why this only fires after
   wifi_scanning.c's on_exit has already stopped the AP-picker scan (wifi_ap_attack_type==13
   leaves wifi_scan_keep_alive false, so that stopscan is unconditional). */

/* Every ~1s tick reprints the SAME target's line again ("essid: N", packets always growing) -
   appending each repeat to a scrolling log (like every other detector here) meant the screen
   kept jumping back to the newest line and burying whatever the user had scrolled up to read.
   Since there is really just one live number to watch, parse it out and redraw it in place
   instead - name once, count big and updated on the spot. */
static char marauder_packet_count_target[MARAUDER_LINE_MAX] = "...";
static char marauder_packet_count_value[16] = "...";

/* AP lines look like "[N][CH:6] ssid rssi" - "select -a" does not follow the AP's channel by
   itself, and packetcount's own channelHop() is a no-op on a fixed channel whenever ChanHop is
   off (the default), so without this the radio just sits wherever a previous scan happened to
   leave it - never the target AP's real channel - and the count shows one stray value then never
   moves again. Same fix as wifi_attack.c's channel -s pre-command. */
static int marauder_gui_scene_wifi_packet_count_parse_channel(const char* line) {
    const char* p = strstr(line, "CH:");
    if(!p) return -1;
    return (int)strtol(p + 3, NULL, 10);
}

static void marauder_gui_scene_wifi_packet_count_send_channel(MarauderGuiApp* app) {
    const char* target = "AP";
    if(app->selected_ap_index >= 0 && (size_t)app->selected_ap_index < app->ap_count) {
        target = app->ap_list[app->selected_ap_index];
    }
    int channel = marauder_gui_scene_wifi_packet_count_parse_channel(target);
    if(channel > 0) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "channel -s %d", channel);
        marauder_uart_send_line(app->uart, cmd);
    }
}

static void marauder_gui_scene_wifi_packet_count_resume_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MARAUDER_RESUME_CUSTOM_EVENT);
    }
}

static void marauder_gui_scene_wifi_packet_count_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        app->wifi_scan_frozen ? marauder_gui_text(app, "Paket Sayaci (Durdu)", "Packet Counter (Stopped)") :
                                 marauder_gui_text(app, "Paket Sayaci", "Packet Counter"));
    widget_add_string_element(
        app->widget, 64, 16, AlignCenter, AlignTop, FontSecondary, marauder_packet_count_target);
    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignTop, FontBigNumbers, marauder_packet_count_value);
    if(app->wifi_scan_frozen) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeCenter,
            marauder_gui_text(app, "Devam Et", "Resume"),
            marauder_gui_scene_wifi_packet_count_resume_button_callback,
            app);
    } else {
        widget_add_string_element(
            app->widget,
            64,
            56,
            AlignCenter,
            AlignTop,
            FontSecondary,
            marauder_gui_text(app, "Geri: Dur", "Back: Stop"));
    }
}

/* Split on the LAST ':' rather than the first - an ESSID can legally contain a colon, but the
   packet count Marauder appends after it never does, so this is the only unambiguous split
   point. */
static void marauder_gui_scene_wifi_packet_count_uart_line(MarauderGuiApp* app, const char* line) {
    const char* sep = strrchr(line, ':');
    if(!sep) return;

    size_t name_len = (size_t)(sep - line);
    if(name_len >= sizeof(marauder_packet_count_target)) {
        name_len = sizeof(marauder_packet_count_target) - 1;
    }
    memcpy(marauder_packet_count_target, line, name_len);
    marauder_packet_count_target[name_len] = '\0';

    const char* value = sep + 1;
    while(*value == ' ') value++;
    strncpy(marauder_packet_count_value, value, sizeof(marauder_packet_count_value) - 1);
    marauder_packet_count_value[sizeof(marauder_packet_count_value) - 1] = '\0';

    marauder_gui_scene_wifi_packet_count_redraw(app);
}

/* Resume (Ok while frozen): the AP is already selected from on_enter and freezing never
   deselects it (only on_exit does, to leave a clean slate on the way out) - so unlike on_enter,
   this must NOT re-send "select -a", or it would toggle the AP back OFF and packetcount would
   go back to reporting nothing. Only the channel (harmless to repeat) and the scan itself need
   restarting. */
static void marauder_gui_scene_wifi_packet_count_resume(MarauderGuiApp* app) {
    marauder_gui_scene_wifi_packet_count_send_channel(app);
    marauder_uart_send_line(app->uart, "packetcount");
    marauder_gui_scene_wifi_packet_count_redraw(app);
}

void marauder_gui_scene_wifi_packet_count_on_enter(void* context) {
    MarauderGuiApp* app = context;

    strncpy(marauder_packet_count_target, "...", sizeof(marauder_packet_count_target) - 1);
    strncpy(marauder_packet_count_value, "...", sizeof(marauder_packet_count_value) - 1);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_wifi_packet_count_uart_line;
    app->resume_handler = marauder_gui_scene_wifi_packet_count_resume;

    marauder_gui_scene_wifi_packet_count_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_wifi_packet_count_send_channel(app);

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
    marauder_uart_send_line(app->uart, cmd);
    marauder_uart_send_line(app->uart, "packetcount");
}

bool marauder_gui_scene_wifi_packet_count_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(app->wifi_scan_frozen) {
            app->wifi_scan_frozen = false;
            marauder_gui_scene_wifi_packet_count_resume(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(!app->wifi_scan_frozen) {
            app->wifi_scan_frozen = true;
            marauder_uart_send_line(app->uart, "stopscan");
            marauder_gui_scene_wifi_packet_count_redraw(app);
        } else {
            scene_manager_previous_scene(app->scene_manager);
        }
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_packet_count_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    /* "select" toggles, so undo the selection this scene turned on, leaving a clean slate for
       the next attack/tool (matches wifi_attack.c's own cleanup for the same side effect). */
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
    marauder_uart_send_line(app->uart, cmd);

    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
