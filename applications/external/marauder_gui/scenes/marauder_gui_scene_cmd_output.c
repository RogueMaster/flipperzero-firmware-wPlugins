#include "../marauder_gui_app_i.h"
#include <string.h>

/* Generic "run app->terminal_cmd and show its streaming output" scene, launched from menus
   (GPS, Tools, ...) for the many Marauder commands that just print text back. Reuses the shared
   terminal_log buffer (only one such scene is ever on screen at a time). Unlike the Terminal
   feature's own output scene there is no "New Command" button - Back simply returns to the menu
   that launched it (sending "stopscan" first, in case the command started a continuous mode). */

static void marauder_gui_scene_cmd_output_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, app->terminal_log);
}

static void marauder_gui_scene_cmd_output_uart_line(MarauderGuiApp* app, const char* line) {
    size_t remaining = sizeof(app->terminal_log) - app->terminal_log_len - 1;
    if(remaining < 2) return;

    size_t line_len = strlen(line);
    size_t to_copy = (line_len > remaining - 1) ? remaining - 1 : line_len;

    memcpy(app->terminal_log + app->terminal_log_len, line, to_copy);
    app->terminal_log_len += to_copy;
    app->terminal_log[app->terminal_log_len++] = '\n';
    app->terminal_log[app->terminal_log_len] = '\0';

    marauder_gui_scene_cmd_output_redraw(app);
}

void marauder_gui_scene_cmd_output_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->terminal_log[0] = '\0';
    app->terminal_log_len = 0;
    app->uart_line_handler = marauder_gui_scene_cmd_output_uart_line;

    marauder_gui_scene_cmd_output_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_uart_send_line(app->uart, app->terminal_cmd);
}

bool marauder_gui_scene_cmd_output_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void marauder_gui_scene_cmd_output_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");
    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
