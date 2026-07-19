#include "../rosetta_i.h"

void rosetta_scene_about_on_enter(void* context) {
    RosettaApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_text_scroll_element(
        widget,
        0,
        0,
        128,
        64,
        "\e#Rosetta\e#\n"
        "The Rosetta Stone for hacking\n"
        "protocols. Learn how it works,\n"
        "then watch it happen.\n"
        " \n"
        "\e#Walkthroughs\e#\n"
        "Animated, step-by-step tours:\n"
        "- Mifare Classic auth (Crypto1)\n"
        "- OOK & PSK modulation\n"
        "- 1-Wire / iButton timing\n"
        "Left / Right to change step.\n"
        " \n"
        "\e#Live Capture\e#\n"
        "Read real hardware, labelled\n"
        "against the lesson:\n"
        "- NFC: UID / SAK / ATQA\n"
        "  (the anticollision a reader\n"
        "  sees before Crypto1 auth)\n"
        "- iButton: 64-bit ROM with an\n"
        "  on-device CRC check\n"
        "- RF: live Sub-GHz envelope\n"
        " \n"
        "All read-only. Nothing is\n"
        "written, cracked or emulated.\n"
        " \n"
        "\e#Ethics\e#\n"
        "Only capture tags and signals\n"
        "you own or may lawfully test.\n"
        " \n"
        "Built by at0m-b0mb\n"
        "github.com/at0m-b0mb\n"
        "v" ROSETTA_VERSION "  -  MIT License");

    view_dispatcher_switch_to_view(app->view_dispatcher, RosettaViewWidget);
}

bool rosetta_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rosetta_scene_about_on_exit(void* context) {
    RosettaApp* app = context;
    widget_reset(app->widget);
}
