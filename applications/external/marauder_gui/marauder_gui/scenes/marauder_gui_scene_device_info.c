#include "../marauder_gui_app_i.h"

/* "info" (no args) just prints a handful of raw text lines once (version, hardware, MAC
   addresses, and - only if actually WiFi-connected - IP/gateway/netmask) and returns; it isn't a
   scan mode (RunInfo() doesn't touch currentScanMode's tick-loop dispatch), so there is nothing
   to stop on the way out, unlike every scan/attack/detector screen. Single Back just leaves. */
static void marauder_gui_scene_device_info_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, marauder_gui_text(app, "Cihaz Bilgisi", "Device Info"));
    widget_add_text_scroll_element(app->widget, 0, 13, 128, 40, app->terminal_log);
}

static void marauder_gui_scene_device_info_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    marauder_gui_log_scan_append(app, line);
    marauder_gui_scene_device_info_redraw(app);
}

void marauder_gui_scene_device_info_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->uart_line_handler = marauder_gui_scene_device_info_uart_line;

    marauder_gui_scene_device_info_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_uart_send_line(app->uart, "info");
}

bool marauder_gui_scene_device_info_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_device_info_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
