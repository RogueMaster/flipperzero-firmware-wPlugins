#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* "clonestamac -s N" is silently ignored while a scan is running (same gate as cloneapmac), so
   this relies on wifi_station_scan.c's on_exit having already sent "stopscan" (unconditional
   there) before this scene's on_enter runs. */

static void marauder_gui_scene_wifi_clone_sta_mac_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        marauder_gui_text(app, "Istemci MAC Klonla", "Clone Client MAC"));
    widget_add_text_box_element(
        app->widget,
        0,
        16,
        128,
        30,
        AlignCenter,
        AlignTop,
        app->last_uart_line[0] != '\0' ? app->last_uart_line :
                                          marauder_gui_text(app, "Gonderiliyor...", "Sending..."),
        false);
}

static void marauder_gui_scene_wifi_clone_sta_mac_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    strncpy(app->last_uart_line, line, MARAUDER_LINE_MAX - 1);
    app->last_uart_line[MARAUDER_LINE_MAX - 1] = '\0';
    marauder_gui_scene_wifi_clone_sta_mac_redraw(app);
}

void marauder_gui_scene_wifi_clone_sta_mac_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->last_uart_line[0] = '\0';
    app->uart_line_handler = marauder_gui_scene_wifi_clone_sta_mac_uart_line;

    marauder_gui_scene_wifi_clone_sta_mac_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "clonestamac -s %d", app->selected_station_index);
    marauder_uart_send_line(app->uart, cmd);
}

bool marauder_gui_scene_wifi_clone_sta_mac_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_clone_sta_mac_on_exit(void* context) {
    MarauderGuiApp* app = context;

    /* wifi_station_scan.c's on_enter unconditionally sent "select -a <ap>" (needed there for
       Targeted Deauth's filterActive() check, but pointless for us - clonestamac doesn't care
       about selection at all) - "select" toggles, so undo it here to leave a clean slate,
       matching wifi_attack.c's own cleanup for the same AP-selection side effect. */
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
    marauder_uart_send_line(app->uart, cmd);

    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
