#include "../argus_i.h"

void argus_scene_about_on_enter(void* context) {
    ArgusApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_icon_element(widget, 2, 2, &I_eye_10px);
    widget_add_string_element(
        widget, 16, 2, AlignLeft, AlignTop, FontPrimary, "Argus v" ARGUS_VERSION);
    widget_add_string_element(
        widget, 16, 14, AlignLeft, AlignTop, FontSecondary, "Wi-Fi guardian");

    widget_add_text_scroll_element(
        widget,
        0,
        26,
        128,
        38,
        "Watches your Wi-Fi for deauth /\n"
        "disassoc attacks and rogue\n"
        "evil-twin APs cloning your SSID.\n"
        " \n"
        "Set your network as the Guarded\n"
        "SSID, open Watch, and Argus\n"
        "alarms when it's under attack.\n"
        " \n"
        "Needs the WiFi devboard (ESP32)\n"
        "flashed with Argus firmware,\n"
        "wired to GPIO 13/14 (UART).\n"
        " \n"
        "A defensive tool - use it only on\n"
        "networks you own or are\n"
        "authorised to test.\n"
        " \n"
        "by at0m-b0mb\n"
        "github.com/at0m-b0mb/Argus-FlipperZero");

    view_dispatcher_switch_to_view(app->view_dispatcher, ArgusViewAbout);
}

bool argus_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void argus_scene_about_on_exit(void* context) {
    ArgusApp* app = context;
    widget_reset(app->widget);
}
