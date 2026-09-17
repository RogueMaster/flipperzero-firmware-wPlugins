#include "../marauder_gui_app_i.h"
void marauder_gui_scene_device_about_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->tick_handler = NULL;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 2, 4, AlignLeft, AlignTop, FontPrimary, "Marauder GUI");
    widget_add_string_element(
        app->widget, 2, 20, AlignLeft, AlignTop, FontSecondary, "ESP32 Marauder");
    widget_add_string_element(
        app->widget,
        2,
        30,
        AlignLeft,
        AlignTop,
        FontSecondary,
        marauder_gui_text(app, "icin Flipper Zero", "GUI app"));
    widget_add_string_element(
        app->widget,
        2,
        40,
        AlignLeft,
        AlignTop,
        FontSecondary,
        marauder_gui_text(app, "GUI uygulamasi", "for Flipper Zero"));
    widget_add_string_element(
        app->widget,
        2,
        48,
        AlignLeft,
        AlignTop,
        FontSecondary,
        marauder_gui_text(app, "Gelistiren: mel4mi", "Developed by: mel4mi"));
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);
}

bool marauder_gui_scene_device_about_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_device_about_on_exit(void* context) {
    MarauderGuiApp* app = context;

    widget_reset(app->widget);
}
