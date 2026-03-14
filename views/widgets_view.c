/*
 * widgets_view.c — widget-based views: delete confirmation, help, about.
 * Adapted from _ref_unitemp/views/Widgets_view.c (icons removed)
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

/* ================== Delete confirmation ================== */

static Sensor* widget_current_sensor;

static uint32_t _delete_exit_callback(void* context) {
    UNUSED(context);
    return ViewSensorActions;
}

static void _delete_click_callback(GuiButtonType result, InputType type, void* context) {
    UNUSED(context);
    if(!widget_current_sensor) return;
    if(result == GuiButtonTypeLeft && type == InputTypeShort) {
        view_sensor_actions_switch(widget_current_sensor);
    }
    if(result == GuiButtonTypeRight && type == InputTypeShort) {
        unitemp_sensor_delete(widget_current_sensor);
        unitemp_sensors_save();
        view_main_switch();
    }
}

void view_widget_delete_switch(Sensor* sensor) {
    if(!sensor) return;
    widget_current_sensor = sensor;
    widget_reset(app->widget);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Cancel", _delete_click_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Delete", _delete_click_callback, app);

    snprintf(app->buff, BUFF_SIZE, "\e#Delete %s?\e#", widget_current_sensor->name);
    widget_add_text_box_element(
        app->widget, 0, 0, 128, 23, AlignCenter, AlignCenter, app->buff, false);

    if(widget_current_sensor->type->interface == &I2C) {
        snprintf(app->buff, BUFF_SIZE, "\e#Type:\e# %s", widget_current_sensor->type->typename);
        widget_add_text_box_element(
            app->widget, 0, 16, 128, 23, AlignLeft, AlignTop, app->buff, false);
        snprintf(
            app->buff,
            BUFF_SIZE,
            "\e#I2C addr:\e# 0x%02X",
            ((I2CSensor*)widget_current_sensor->instance)->currentI2CAdr >> 1);
        widget_add_text_box_element(
            app->widget, 0, 28, 128, 23, AlignLeft, AlignTop, app->buff, false);
    } else if(widget_current_sensor->type->interface == &SINGLE_WIRE) {
        snprintf(app->buff, BUFF_SIZE, "\e#Type:\e# %s", widget_current_sensor->type->typename);
        widget_add_text_box_element(
            app->widget, 0, 16, 128, 23, AlignLeft, AlignTop, app->buff, false);
        snprintf(
            app->buff,
            BUFF_SIZE,
            "\e#GPIO:\e# %s",
            ((SingleWireSensor*)widget_current_sensor->instance)->gpio->name);
        widget_add_text_box_element(
            app->widget, 0, 28, 128, 23, AlignLeft, AlignTop, app->buff, false);
    } else {
        snprintf(app->buff, BUFF_SIZE, "\e#Type:\e# %s", widget_current_sensor->type->typename);
        widget_add_text_box_element(
            app->widget, 0, 16, 128, 23, AlignLeft, AlignTop, app->buff, false);
    }

    view_set_previous_callback(widget_get_view(app->widget), _delete_exit_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

/* ========================== Help ======================================== */

static uint32_t _help_about_prev_view = ViewMain;

static uint32_t _help_exit_callback(void* context) {
    UNUSED(context);
    return _help_about_prev_view;
}

void view_widget_help_switch(void) {
    _help_about_prev_view = ViewMain;
    widget_reset(app->widget);
    widget_add_frame_element(app->widget, 0, 0, 128, 63, 7);
    widget_add_frame_element(app->widget, 0, 0, 128, 64, 7);
    widget_add_string_multiline_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary,
        "CO2 Monitor\nBy flipper-air-stats\ngithub.com");
    view_set_previous_callback(widget_get_view(app->widget), _help_exit_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

void view_widget_help_from_settings_switch(void) {
    _help_about_prev_view = ViewSettingsMenu;
    view_widget_help_switch();
}

/* ========================== About ======================================== */

void view_widget_about_switch(void) {
    _help_about_prev_view = ViewMainMenu;
    widget_reset(app->widget);
    widget_add_frame_element(app->widget, 0, 0, 128, 63, 7);
    widget_add_frame_element(app->widget, 0, 0, 128, 64, 7);
    widget_add_text_box_element(
        app->widget, 0, 4, 128, 12, AlignCenter, AlignCenter, "Air Stats v0.1", false);
    widget_add_text_scroll_element(
        app->widget, 4, 16, 121, 44,
        "CO2 + climate monitor\nfor Flipper Zero\nAuthor: ivatikhonov\ngithub.com/ivatikhonov\n/flipper-air-stats");
    view_set_previous_callback(widget_get_view(app->widget), _help_exit_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

void view_widget_about_from_settings_switch(void) {
    _help_about_prev_view = ViewSettingsMenu;
    view_widget_about_switch();
}
