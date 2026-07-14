#include "../wendigo_app_i.h"

void wendigo_scene_help_widget_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_help_widget_callback()");
    WendigoApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_help_widget_callback()");
}

void wendigo_scene_help_on_enter(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_help_on_enter()");
    WendigoApp* app = context;
    app->current_view = WendigoAppViewHelp;

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(
        temp_str,
        "           Wendigo for Flipper\n\nFind me on GitHub @chris-bc\n\nThis general-purpose sniffer\n    allows you to monitor BT,\nBLE and WiFi radios, alerting\nyou when the signal strength\nof devices of interest changes.\n\n   This allows for rudimentary\n        device tracking, with a\n         \"homing\" feature to be\n            developed over time.\n\n          This app is based on\n    @cool4uma's handy UART\n        Terminal, which in turn\n                  borrows from\n      @0xchocolate's fabulous\n                  Marauder app.\n\n  We all stand on the shoulders\n    of those who came before!\n\n");
    furi_string_cat_printf(temp_str, "Press BACK to return\n");

    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!                                                      \e!\n",
        false);
    widget_add_text_box_element(
        app->widget,
        0,
        2,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!         WENDIGO            \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewHelp);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_help_on_enter()");
}

bool wendigo_scene_help_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_help_on_event()");
    WendigoApp* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_help_on_event()");
    return consumed;
}

void wendigo_scene_help_on_exit(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_help_on_exit()");
    WendigoApp* app = context;
    // Clear views
    widget_reset(app->widget);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_help_on_exit()");
}
