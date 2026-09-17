#include "../marauder_gui_app_i.h"

/* "randapmac"/"randstamac" just pick a new random MAC and print one confirmation line
   ("Setting AP MAC: xx:xx:xx:xx:xx:xx" / "Setting STA MAC: ...") - not a scan mode, nothing to
   stop. Note (confirmed via CommandLine.cpp): this command is silently ignored if any scan is
   currently running, but nothing scan-related is active on the way into this menu, so that's
   not a concern here. */

static void marauder_gui_scene_wifi_random_mac_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        app->wifi_mac_random_is_ap ? marauder_gui_text(app, "Rastgele AP MAC", "Random AP MAC") :
                                      marauder_gui_text(app, "Rastgele Istemci MAC", "Random Client MAC"));
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

static void marauder_gui_scene_wifi_random_mac_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    strncpy(app->last_uart_line, line, MARAUDER_LINE_MAX - 1);
    app->last_uart_line[MARAUDER_LINE_MAX - 1] = '\0';
    marauder_gui_scene_wifi_random_mac_redraw(app);
}

void marauder_gui_scene_wifi_random_mac_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->last_uart_line[0] = '\0';
    app->uart_line_handler = marauder_gui_scene_wifi_random_mac_uart_line;

    marauder_gui_scene_wifi_random_mac_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_uart_send_line(app->uart, app->wifi_mac_random_is_ap ? "randapmac" : "randstamac");
}

bool marauder_gui_scene_wifi_random_mac_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_random_mac_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
