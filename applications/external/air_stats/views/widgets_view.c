/*
 * widgets_view.c — widget-based views: about.
 */
#include "../air_stats_i.h"

void view_widgets_alloc(void) {
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ViewWidget, widget_get_view(app->widget));
}

void view_widgets_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    widget_free(app->widget);
}

/* ========================== About ======================================== */

static uint32_t _about_exit_callback(void* context) {
    UNUSED(context);
    return ViewMainMenu;
}

void view_widget_about_switch(void) {
    widget_reset(app->widget);
    widget_add_frame_element(app->widget, 0, 0, 128, 63, 7);
    widget_add_frame_element(app->widget, 0, 0, 128, 64, 7);
    widget_add_text_box_element(
        app->widget, 0, 4, 128, 12, AlignCenter, AlignCenter, "Air Stats v0.1", false);
    widget_add_text_scroll_element(
        app->widget, 4, 16, 121, 44,
        "CO2 + climate monitor\nfor Flipper Zero\nAuthor: ivatikhonov\ngithub.com/ivatikhonov\n/flipper-air-stats");
    view_set_previous_callback(widget_get_view(app->widget), _about_exit_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}
