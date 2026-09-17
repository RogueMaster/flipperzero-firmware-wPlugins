#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* "info -a N" (RunAPInfo) prints 9 fixed "Label: value" lines once (not a scan mode, nothing to
   stop on exit) - rather than dump them raw into a scrolling terminal-style log (padded with
   Marauder's own alignment spaces, awkward on a proportional font), parse each into a small
   fixed set of fields and lay them out as a compact one-screen summary, two per line where they
   fit, using short Turkish labels. */
typedef struct {
    char ssid[MARAUDER_LINE_MAX];
    char bssid[20];
    char channel[8];
    char rssi[8];
    char stations[8];
    char eapol[8];
    char security[24];
} MarauderApInfoFields;

static MarauderApInfoFields marauder_ap_info_fields;

static void marauder_gui_scene_wifi_ap_info_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, marauder_gui_text(app, "AP Bilgisi", "AP Info"));

    widget_add_string_element(
        app->widget, 2, 15, AlignLeft, AlignTop, FontSecondary, marauder_ap_info_fields.ssid);
    widget_add_string_element(
        app->widget, 2, 25, AlignLeft, AlignTop, FontSecondary, marauder_ap_info_fields.bssid);

    char line[48];
    snprintf(
        line,
        sizeof(line),
        marauder_gui_text(app, "Kanal: %s   RSSI: %s", "Channel: %s   RSSI: %s"),
        marauder_ap_info_fields.channel,
        marauder_ap_info_fields.rssi);
    widget_add_string_element(app->widget, 2, 37, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(
        line, sizeof(line), marauder_gui_text(app, "Guvenlik: %s", "Security: %s"), marauder_ap_info_fields.security);
    widget_add_string_element(app->widget, 2, 47, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(
        line,
        sizeof(line),
        marauder_gui_text(app, "Istemci: %s   EAPOL: %s", "Client: %s   EAPOL: %s"),
        marauder_ap_info_fields.stations,
        marauder_ap_info_fields.eapol);
    widget_add_string_element(app->widget, 2, 57, AlignLeft, AlignTop, FontSecondary, line);
}

/* Every line here is "Label: value" (Marauder pads/right-aligns the label with leading spaces
   for its own monospace serial output - trimmed away here). */
static void marauder_gui_scene_wifi_ap_info_uart_line(MarauderGuiApp* app, const char* line) {
    const char* sep = strchr(line, ':');
    if(!sep) return;

    const char* label = line;
    while(*label == ' ') label++;
    size_t label_len = (size_t)(sep - label);
    while(label_len > 0 && label[label_len - 1] == ' ') label_len--;

    const char* value = sep + 1;
    while(*value == ' ') value++;

    if(label_len == 5 && strncmp(label, "ESSID", 5) == 0) {
        snprintf(
            marauder_ap_info_fields.ssid, sizeof(marauder_ap_info_fields.ssid), "SSID: %s", value);
    } else if(label_len == 5 && strncmp(label, "BSSID", 5) == 0) {
        snprintf(
            marauder_ap_info_fields.bssid,
            sizeof(marauder_ap_info_fields.bssid),
            "BSSID: %s",
            value);
    } else if(label_len == 7 && strncmp(label, "Channel", 7) == 0) {
        strncpy(marauder_ap_info_fields.channel, value, sizeof(marauder_ap_info_fields.channel) - 1);
    } else if(label_len == 4 && strncmp(label, "RSSI", 4) == 0) {
        strncpy(marauder_ap_info_fields.rssi, value, sizeof(marauder_ap_info_fields.rssi) - 1);
    } else if(label_len == 8 && strncmp(label, "Stations", 8) == 0) {
        strncpy(
            marauder_ap_info_fields.stations, value, sizeof(marauder_ap_info_fields.stations) - 1);
    } else if(label_len == 14 && strncmp(label, "Complete EAPOL", 14) == 0) {
        strncpy(
            marauder_ap_info_fields.eapol,
            strcmp(value, "TRUE") == 0 ? marauder_gui_text(app, "Evet", "Yes") :
                                          marauder_gui_text(app, "Hayir", "No"),
            sizeof(marauder_ap_info_fields.eapol) - 1);
    } else if(label_len == 8 && strncmp(label, "Security", 8) == 0) {
        strncpy(
            marauder_ap_info_fields.security, value, sizeof(marauder_ap_info_fields.security) - 1);
    } else {
        /* "Frames"/"Brand" aren't shown (screen is already full) - ignore anything else. */
        return;
    }

    marauder_gui_scene_wifi_ap_info_redraw(app);
}

void marauder_gui_scene_wifi_ap_info_on_enter(void* context) {
    MarauderGuiApp* app = context;

    memset(&marauder_ap_info_fields, 0, sizeof(marauder_ap_info_fields));
    snprintf(marauder_ap_info_fields.ssid, sizeof(marauder_ap_info_fields.ssid), "SSID: ...");
    strncpy(marauder_ap_info_fields.bssid, "BSSID: ...", sizeof(marauder_ap_info_fields.bssid) - 1);
    strncpy(marauder_ap_info_fields.channel, "...", sizeof(marauder_ap_info_fields.channel) - 1);
    strncpy(marauder_ap_info_fields.rssi, "...", sizeof(marauder_ap_info_fields.rssi) - 1);
    strncpy(marauder_ap_info_fields.stations, "...", sizeof(marauder_ap_info_fields.stations) - 1);
    strncpy(marauder_ap_info_fields.eapol, "...", sizeof(marauder_ap_info_fields.eapol) - 1);
    strncpy(marauder_ap_info_fields.security, "...", sizeof(marauder_ap_info_fields.security) - 1);

    app->uart_line_handler = marauder_gui_scene_wifi_ap_info_uart_line;

    marauder_gui_scene_wifi_ap_info_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "info -a %d", app->selected_ap_index);
    marauder_uart_send_line(app->uart, cmd);
}

bool marauder_gui_scene_wifi_ap_info_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_ap_info_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
