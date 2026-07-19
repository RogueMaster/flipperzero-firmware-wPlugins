#include "../glitch_trigger_i.h"

void glitch_scene_about_on_enter(void* context) {
    GlitchApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_text_scroll_element(
        widget,
        0,
        0,
        128,
        64,
        "\e#Glitch Trigger\e#\n"
        "A precise, configurable GPIO\n"
        "pulse generator for voltage-\n"
        "glitch / fault-injection study\n"
        "on your own dev boards.\n"
        " \n"
        "\e#The shot\e#\n"
        "trigger -> delay -> pulse(s)\n"
        "- Delay: trigger-to-glitch\n"
        "- Width: the glitch itself\n"
        "- Pulses / Gap: bursts\n"
        "- Polarity: Active-High/Low\n"
        "Timing is cycle-accurate\n"
        "(DWT, interrupts masked).\n"
        " \n"
        "\e#Triggering\e#\n"
        "- Manual: OK fires\n"
        "- External: an edge on the\n"
        "  trig-in pin fires (ISR)\n"
        "- Repeat: free-running\n"
        " \n"
        "\e#Sweep\e#\n"
        "Walk the width across a range\n"
        "to hunt the fault window; OK\n"
        "marks a hit.\n"
        " \n"
        "\e#Hardware\e#\n"
        "The pin is 3V3 push-pull. It\n"
        "cannot switch a target rail by\n"
        "itself - drive a MOSFET /\n"
        "gate-driver crowbar. Share a\n"
        "common ground. See Wiring.\n"
        " \n"
        "\e#Ethics\e#\n"
        "Fault injection can damage\n"
        "hardware and corrupt data.\n"
        "Only test boards you own and\n"
        "are authorised to test.\n"
        " \n"
        "Built by at0m-b0mb\n"
        "github.com/at0m-b0mb\n"
        "v" GLITCH_VERSION "  -  MIT License");

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewWidget);
}

bool glitch_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void glitch_scene_about_on_exit(void* context) {
    GlitchApp* app = context;
    widget_reset(app->widget);
}
