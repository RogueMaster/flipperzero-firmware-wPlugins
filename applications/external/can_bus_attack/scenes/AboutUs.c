#include "../app_user.h"

static uint32_t state = 0;

static void button_callback(GuiButtonType result, InputType input, void* context) {
    App* app = context;

    if(input == InputTypePress) {
        if(result == GuiButtonTypeRight) {
            if(state < 4) state = state + 1;
            scene_manager_handle_custom_event(app->scene_manager, ButtonGetPressed);
        }
        if(result == GuiButtonTypeLeft) {
            if(state > 0) state = state - 1;
            scene_manager_handle_custom_event(app->scene_manager, ButtonGetPressed);
        }
    }
}

static void draw_present_view(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 40, 1, &I_EC48x26);
    widget_add_string_multiline_element(
        app->widget, 65, 40, AlignCenter, AlignCenter, FontPrimary, "ELECTRONIC CATS \nAND");
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", button_callback, app);
}

static void draw_present_view2(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 26, 6, &I_Icons_etsi_uma);
    widget_add_string_multiline_element(
        app->widget, 65, 40, AlignCenter, AlignCenter, FontPrimary, "\n Presents:");

    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", button_callback, app);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", button_callback, app);
}

static void draw_can_app_view(App* app) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget, 65, 20, AlignCenter, AlignCenter, FontPrimary, "CANBUS ATTACK APP");
    widget_add_string_element(
        app->widget, 65, 35, AlignCenter, AlignCenter, FontSecondary, PROGRAM_VERSION);

    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", button_callback, app);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", button_callback, app);
}

static void draw_license_view(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 65, 20, AlignCenter, AlignCenter, FontPrimary, "MIT LICENSE");
    widget_add_string_element(
        app->widget, 65, 35, AlignCenter, AlignCenter, FontSecondary, "Copyright(c) 2024");
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", button_callback, app);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", button_callback, app);
}

void draw_more_info_view(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 10, 5, AlignLeft, AlignCenter, FontPrimary, "More info:");
    widget_add_string_element(
        app->widget, 64, 15, AlignCenter, AlignTop, FontSecondary, "github.com/ElectronicCats");
    widget_add_string_element(
        app->widget, 64, 27, AlignCenter, AlignTop, FontSecondary, "/canbus_app");
    widget_add_string_element(
        app->widget, 64, 39, AlignCenter, AlignTop, FontSecondary, "github.com/jorgepnaranjo25");
    widget_add_string_element(
        app->widget, 64, 51, AlignCenter, AlignTop, FontSecondary, "/CAN-Bus-Attack");
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", button_callback, app);
}

void app_scene_about_us_on_enter(void* context) {
    App* app = context;
    state = 0;
    draw_present_view(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool app_scene_about_us_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    // uint32_t state_2 = state;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(state) {
        case 0:
            draw_present_view(app);
            break;
        case 1:
            draw_present_view2(app);
            break;
        case 2:
            draw_can_app_view(app);
            break;
        case 3:
            draw_license_view(app);
            break;
        case 4:
            draw_more_info_view(app);
            break;
        default:
            break;
        }
        consumed = true;
    }
    return consumed;
}

void app_scene_about_us_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}
