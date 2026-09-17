#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

enum {
    BtTrackerStatusEventStop,
};

static void marauder_gui_scene_bt_tracker_status_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BtTrackerStatusEventStop);
    }
}

static void marauder_gui_scene_bt_tracker_status_redraw(MarauderGuiApp* app) {
    const char* target = "Tracker";
    if(app->selected_tracker_index >= 0 && (size_t)app->selected_tracker_index < app->ap_count) {
        target = app->ap_list[app->selected_tracker_index];
    }

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        app->bt_tracker_action == 0 ? marauder_gui_text(app, "Ses Calma", "Playing Sound") : "Spoof");
    widget_add_text_box_element(app->widget, 0, 13, 128, 14, AlignCenter, AlignTop, target, true);
    widget_add_text_box_element(
        app->widget, 0, 28, 128, 20, AlignCenter, AlignTop, app->last_uart_line, true);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        marauder_gui_text(app, "Tamam", "OK"),
        marauder_gui_scene_bt_tracker_status_button_callback,
        app);
}

static void marauder_gui_scene_bt_tracker_status_uart_line(MarauderGuiApp* app, const char* line) {
    strncpy(app->last_uart_line, line, MARAUDER_LINE_MAX - 1);
    app->last_uart_line[MARAUDER_LINE_MAX - 1] = '\0';
    marauder_gui_scene_bt_tracker_status_redraw(app);
}

void marauder_gui_scene_bt_tracker_status_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->last_uart_line[0] = '\0';
    app->uart_line_handler = marauder_gui_scene_bt_tracker_status_uart_line;

    marauder_gui_scene_bt_tracker_status_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    char cmd[32];
    if(app->bt_tracker_action == 0) {
        snprintf(cmd, sizeof(cmd), "findmy -t %d", app->selected_tracker_index);
    } else {
        snprintf(cmd, sizeof(cmd), "spoofat -t %d", app->selected_tracker_index);
    }
    marauder_uart_send_line(app->uart, cmd);
}

bool marauder_gui_scene_bt_tracker_status_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom && event.event == BtTrackerStatusEventStop) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_bt_tracker_status_on_exit(void* context) {
    MarauderGuiApp* app = context;

    /* Harmless no-op for "findmy" (one-shot, no scan mode); required to actually stop
       "spoofat"'s continuous broadcast. */
    marauder_uart_send_line(app->uart, "stopscan");

    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
