#include "../marauder_gui_app_i.h"
#include <string.h>

enum {
    TerminalOutputEventNewCommand,
};

static void marauder_gui_scene_terminal_output_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TerminalOutputEventNewCommand);
    }
}

static void marauder_gui_scene_terminal_output_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, app->terminal_log);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        marauder_gui_text(app, "Yeni Komut", "New Command"),
        marauder_gui_scene_terminal_output_button_callback,
        app);
}

/* Unlike the other status scenes (which only show Marauder's *last* line), this accumulates
   every line into a scrolling log - the whole point of a raw terminal is seeing everything a
   command prints back, e.g. a full "ls" directory listing. */
static void marauder_gui_scene_terminal_output_uart_line(MarauderGuiApp* app, const char* line) {
    size_t remaining = sizeof(app->terminal_log) - app->terminal_log_len - 1;
    if(remaining < 2) return;

    size_t line_len = strlen(line);
    size_t to_copy = (line_len > remaining - 1) ? remaining - 1 : line_len;

    memcpy(app->terminal_log + app->terminal_log_len, line, to_copy);
    app->terminal_log_len += to_copy;
    app->terminal_log[app->terminal_log_len++] = '\n';
    app->terminal_log[app->terminal_log_len] = '\0';

    marauder_gui_scene_terminal_output_redraw(app);
}

void marauder_gui_scene_terminal_output_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->terminal_log[0] = '\0';
    app->terminal_log_len = 0;
    app->uart_line_handler = marauder_gui_scene_terminal_output_uart_line;

    marauder_gui_scene_terminal_output_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_uart_send_line(app->uart, app->terminal_cmd);
}

bool marauder_gui_scene_terminal_output_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == TerminalOutputEventNewCommand) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_terminal_output_on_exit(void* context) {
    MarauderGuiApp* app = context;

    /* Safety net in case the typed command started a continuous scan mode (e.g. "sniffbt") */
    marauder_uart_send_line(app->uart, "stopscan");

    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
