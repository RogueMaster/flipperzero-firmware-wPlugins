#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* joinWiFi() (WiFiScan.cpp) blocks the ESP32's whole loop for up to ~10s (20 retries x 500ms)
   while it tries to associate, printing a dot per retry with no line breaks in between - so our
   line-based reader only flushes that as one (possibly truncated) blob once a newline finally
   arrives. On failure it explicitly prints "Could not connect to WiFi network" (or, for the
   "join -s" no-saved-credentials case, "There are no saved WiFi credentials") - on success there
   is no explicit confirmation message at all, so success is inferred from the absence of either
   failure line after waiting comfortably longer than the worst-case timeout. */
#define WIFI_JOIN_WAIT_TICKS 120 /* ~12s at the app's 100ms tick period */

enum {
    WifiJoinStatusEventStop,
};

/* AP list lines look like "[N][CH:6] ssid rssi" - pull out just "ssid" for a friendly
   "connected to X" label. Heuristic (strips the last space-separated token if it looks like the
   trailing rssi number), not bulletproof against SSIDs containing spaces AND ending in a number,
   but this is only ever used for a display label, never for a protocol command. */
static void marauder_gui_scene_wifi_join_extract_ssid(const char* line, char* out, size_t out_size) {
    const char* p = line;
    if(*p == '[') {
        p = strchr(p, ']');
        if(p) p++;
    }
    if(p && *p == '[') {
        p = strchr(p, ']');
        if(p) p++;
    }
    while(p && *p == ' ') p++;
    if(!p || !*p) p = line;

    strncpy(out, p, out_size - 1);
    out[out_size - 1] = '\0';

    char* last_space = strrchr(out, ' ');
    if(last_space && last_space[1] != '\0') {
        const char* rest = last_space + 1;
        bool is_num = true;
        for(const char* q = rest + (*rest == '-' ? 1 : 0); *q; q++) {
            if(*q < '0' || *q > '9') {
                is_num = false;
                break;
            }
        }
        if(is_num) *last_space = '\0';
    }
}

static void marauder_gui_scene_wifi_join_status_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WifiJoinStatusEventStop);
    }
}

static void marauder_gui_scene_wifi_join_status_redraw(MarauderGuiApp* app) {
    const char* target = app->wifi_pending_label;

    const char* status;
    if(!app->wifi_join_done) {
        status = marauder_gui_text(app, "Baglaniyor... (~10sn)", "Connecting... (~10s)");
    } else if(app->wifi_join_failed) {
        status = marauder_gui_text(app, "Baglanti basarisiz", "Connection failed");
    } else {
        status = marauder_gui_text(app, "Baglandi (varsayilan)", "Connected (assumed)");
    }

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, marauder_gui_text(app, "WiFi Baglan", "Join WiFi"));
    widget_add_text_box_element(app->widget, 0, 13, 128, 14, AlignCenter, AlignTop, target, true);
    widget_add_text_box_element(app->widget, 0, 28, 128, 12, AlignCenter, AlignTop, status, true);
    widget_add_text_box_element(
        app->widget, 0, 40, 128, 12, AlignCenter, AlignTop, app->last_uart_line, true);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        marauder_gui_text(app, "Tamam", "OK"),
        marauder_gui_scene_wifi_join_status_button_callback,
        app);
}

static void marauder_gui_scene_wifi_join_status_uart_line(MarauderGuiApp* app, const char* line) {
    strncpy(app->last_uart_line, line, MARAUDER_LINE_MAX - 1);
    app->last_uart_line[MARAUDER_LINE_MAX - 1] = '\0';

    if(strstr(line, "Could not connect") || strstr(line, "no saved WiFi credentials")) {
        app->wifi_join_failed = true;
    }

    /* "settings" (sent before "join -s") dumps every setting as three lines: "Name: X",
       "Type: Y", "Value: Z". We wait for the "Value: " line that immediately follows
       "Name: ClientSSID" (skipping over the "Type: String" line in between) to learn the
       real saved SSID, since the join itself never echoes it back. */
    if(strcmp(line, "Name: ClientSSID") == 0) {
        app->wifi_settings_awaiting_ssid = true;
    } else if(app->wifi_settings_awaiting_ssid && strncmp(line, "Value: ", 7) == 0) {
        app->wifi_settings_awaiting_ssid = false;
        if(line[7] != '\0') {
            strncpy(app->wifi_pending_label, line + 7, sizeof(app->wifi_pending_label) - 1);
            app->wifi_pending_label[sizeof(app->wifi_pending_label) - 1] = '\0';
        }
    }

    if(!app->wifi_join_done) {
        marauder_gui_scene_wifi_join_status_redraw(app);
    }
}

static void marauder_gui_scene_wifi_join_status_tick(MarauderGuiApp* app) {
    if(app->wifi_join_done) return;

    app->wifi_join_wait_ticks++;
    if(app->wifi_join_failed || app->wifi_join_wait_ticks >= WIFI_JOIN_WAIT_TICKS) {
        app->wifi_join_done = true;

        if(app->wifi_join_failed) {
            app->wifi_is_connected = false;
        } else {
            app->wifi_is_connected = true;
            strncpy(
                app->wifi_connected_label,
                app->wifi_pending_label,
                sizeof(app->wifi_connected_label) - 1);
            app->wifi_connected_label[sizeof(app->wifi_connected_label) - 1] = '\0';
        }

        marauder_gui_scene_wifi_join_status_redraw(app);
    }
}

void marauder_gui_scene_wifi_join_status_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->last_uart_line[0] = '\0';
    app->wifi_join_failed = false;
    app->wifi_join_done = false;
    app->wifi_join_wait_ticks = 0;
    app->wifi_settings_awaiting_ssid = false;

    app->uart_line_handler = marauder_gui_scene_wifi_join_status_uart_line;
    app->tick_handler = marauder_gui_scene_wifi_join_status_tick;

    marauder_gui_scene_wifi_join_status_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    if(app->wifi_join_use_saved) {
        /* Fallback label in case the "settings" parse below doesn't land in time (older
           firmware without the command, or a saved-credentials slot that was never actually
           set) - overwritten by the real SSID as soon as its "Value: " line arrives. */
        if(app->wifi_connected_label[0] == '\0') {
            strncpy(
                app->wifi_pending_label,
                marauder_gui_text(app, "Kayitli Ag", "Saved Network"),
                sizeof(app->wifi_pending_label) - 1);
            app->wifi_pending_label[sizeof(app->wifi_pending_label) - 1] = '\0';
        } else {
            strncpy(
                app->wifi_pending_label, app->wifi_connected_label, sizeof(app->wifi_pending_label) - 1);
            app->wifi_pending_label[sizeof(app->wifi_pending_label) - 1] = '\0';
        }

        /* "join -s" itself never echoes the SSID, and CommandLine.cpp processes commands one
           at a time off the serial buffer, so sending "settings" first guarantees its
           "Name: ClientSSID" / "Value: X" lines are fully printed and parsed (see the
           uart_line handler above) before "join -s" starts its blocking ~10s connect loop. */
        marauder_uart_send_line(app->uart, "settings");
        marauder_uart_send_line(app->uart, "join -s");
    } else {
        const char* target = "AP";
        if(app->selected_ap_index >= 0 && (size_t)app->selected_ap_index < app->ap_count) {
            target = app->ap_list[app->selected_ap_index];
        }
        marauder_gui_scene_wifi_join_extract_ssid(
            target, app->wifi_pending_label, sizeof(app->wifi_pending_label));

        char cmd[96];
        snprintf(
            cmd, sizeof(cmd), "join -a %d -p \"%s\"", app->selected_ap_index, app->wifi_join_password);
        marauder_uart_send_line(app->uart, cmd);
    }
}

bool marauder_gui_scene_wifi_join_status_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom && event.event == WifiJoinStatusEventStop) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_join_status_on_exit(void* context) {
    MarauderGuiApp* app = context;

    /* Deliberately NOT sending "stopscan" here, unlike every other status scene: StopScan()
       calls shutdownWiFi(), which - if `wifi_connected` isn't set yet (e.g. join actually
       failed, or the flag just hasn't been updated by the firmware's own loop() the instant we
       exit) - runs esp_wifi_stop()/esp_wifi_restore()/esp_wifi_deinit()/esp_netif_deinit(),
       fully tearing down the WiFi driver. WiFi.begin() afterward (what the next join attempt
       does) can't recover from that - only a device reboot can. Confirmed the hard way: this
       bricked WiFi on the user's device until they power-cycled it. Joining has nothing that
       actually needs stopping, so just don't send anything on the way out. */
    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
    widget_reset(app->widget);
}
