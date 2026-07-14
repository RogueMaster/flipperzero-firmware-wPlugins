#include "../gemini_app_i.h"

void gemini_scene_help_widget_callback(GuiButtonType result, InputType type, void* context) {
    GeminiApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}
#define GEMINI_APP_DESCRIPTION                                                                                                                                           \
    "This is an app to interact with Google's Gemini AI. "                                                                                                               \
    "It's pretty intuitive, it will ask you to connect to a wifi network if you haven't already and to select the name you'd like to use (you can change it later).\n\n" \
    "You'll need an API key to be able to interact with Gemini, more details on how to get it can be found in the repository "                                           \
    "https://github.com/d4rks1d33/Gemini-Flipper\n\n"                                                                                                                    \
    "Special thanks to @codeallnight (Derek Jamison) who made most of this app and without his help it would never have gotten this far.\n\n"                            \
    "Also special thanks to all the Momentum members who supported this project and collaborated to help make this app reach all of you."

void gemini_scene_help_on_enter(void* context) {
    GeminiApp* app = context;

    widget_reset(app->widget);

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
        "\e#\e!        Gemini App       \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, GEMINI_APP_DESCRIPTION);

    view_dispatcher_switch_to_view(app->view_dispatcher, GeminiViewWidget);
}

bool gemini_scene_help_on_event(void* context, SceneManagerEvent event) {
    GeminiApp* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void gemini_scene_help_on_exit(void* context) {
    GeminiApp* app = context;

    widget_reset(app->widget);
}
