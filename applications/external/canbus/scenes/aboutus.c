#include "../app_user.h"

static uint32_t state = 0;

static void button_callback(GuiButtonType result, InputType input, void* context) {
    App* app = context;

    if(input == InputTypePress) {
        if(result == GuiButtonTypeRight) {
            state = state + 1;
            scene_manager_handle_custom_event(app->scene_manager, ButtonGetPressed);
        }
        if(result == GuiButtonTypeLeft) {
            state = state - 1;
            scene_manager_handle_custom_event(app->scene_manager, ButtonGetPressed);
        }
    }
}

static void draw_present_view(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 40, 1, &I_EC48x26);
    widget_add_string_multiline_element(
        app->widget, 65, 40, AlignCenter, AlignCenter, FontPrimary, "ELECTRONIC CATS \n Presents:");
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", button_callback, app);
}

static void draw_can_app_view(App* app) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget, 65, 20, AlignCenter, AlignCenter, FontPrimary, "CANBUS APP");
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
    widget_add_icon_element(app->widget, 48, 15, &I_qrcode);
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

    uint32_t state_2 = state;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(state_2) {
        case 0:
            draw_present_view(app);
            break;
        case 1:
            draw_can_app_view(app);
            break;
        case 2:
            draw_license_view(app);
            break;
        case 3:
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
