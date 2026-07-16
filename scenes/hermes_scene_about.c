#include "../hermes_i.h"

void hermes_scene_about_on_enter(void* context) {
    HermesApp* app = context;
    Widget* widget = app->widget;

    widget_add_text_scroll_element(
        widget,
        0,
        0,
        128,
        64,
        "\e#Hermes " HERMES_VERSION "\e#\n"
        "Every wire is talking.\n"
        "Hermes tells you what\n"
        "it's saying.\n\n"
        "\e#How it works\e#\n"
        "1. It times the edges on\n"
        "the RX pin with the CPU\n"
        "cycle counter and fits a\n"
        "bit time to them.\n"
        "2. It re-opens the line as\n"
        "a real UART and checks the\n"
        "guess against hardware\n"
        "framing errors.\n"
        "So the answer is measured,\n"
        "not assumed.\n\n"
        "\e#Wiring\e#\n"
        "Flipper RX <- target TX.\n"
        "GND to GND, always.\n"
        "3.3V logic only. RS-232\n"
        "at +/-12V will kill it:\n"
        "use a MAX3232.\n\n"
        "\e#Limits\e#\n"
        "Edge timing is good to\n"
        "~230400. Above that the\n"
        "interrupt cannot keep up,\n"
        "so Hermes sweeps the rate\n"
        "table on the UART instead,\n"
        "which has no such limit.\n\n"
        "\e#Ethics\e#\n"
        "For your own boards, or\n"
        "ones you are authorised to\n"
        "test. It listens by\n"
        "default and only drives\n"
        "the line when you type.\n\n"
        "MIT (c) 2026 at0m-b0mb\n"
        "github.com/at0m-b0mb/\n"
        "Hermes-FlipperZero");

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewWidget);
}

bool hermes_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hermes_scene_about_on_exit(void* context) {
    HermesApp* app = context;
    widget_reset(app->widget);
}
