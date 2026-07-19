#include "../faraday_i.h"

void faraday_scene_about_on_enter(void* context) {
    FaradayApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    widget_add_text_scroll_element(
        widget,
        0,
        0,
        128,
        64,
        "\e#Faraday " FARADAY_VERSION "\e#\n"
        "Prove your signal-blocking\n"
        "pouch actually works.\n"
        "\n"
        "\e#The test\e#\n"
        "Every test is two captures and\n"
        "a grade:\n"
        "1. BASELINE - the signal in the\n"
        "   open air.\n"
        "2. SHIELDED - the same signal\n"
        "   with the pouch sealed.\n"
        "The gap is the attenuation.\n"
        "\n"
        "\e#Sub-GHz (key fob)\e#\n"
        "The CC1101 measures your fob's\n"
        "carrier in real dBm. Press the\n"
        "fob in the open, lock it, seal\n"
        "the fob in the pouch, press it\n"
        "again. The dB drop is graded\n"
        "A+ to F.\n"
        "\n"
        "\e#NFC (card)\e#\n"
        "Hold the Flipper in a reader's\n"
        "13.56 MHz field (a phone doing\n"
        "NFC works), lock the baseline,\n"
        "then seal the Flipper in the\n"
        "pouch and measure again. The\n"
        "score is how much of the\n"
        "interrogation field the pouch\n"
        "keeps out.\n"
        "\n"
        "\e#Leak hunt\e#\n"
        "A grade tells you a pouch leaks.\n"
        "This finds WHERE. Seal the fob,\n"
        "hold its button, and sweep the\n"
        "Flipper along the seams, zip and\n"
        "corners. The meter, the warmer/\n"
        "colder word and the clicks all\n"
        "peak over the escaping spot.\n"
        "OK resets the peak to re-sweep.\n"
        "\n"
        "\e#Saved results\e#\n"
        "Every finished test is appended\n"
        "to a CSV on the SD card, so you\n"
        "can measure three pouches and\n"
        "compare them later instead of\n"
        "trusting memory.\n"
        "\n"
        "\e#Honest limits\e#\n"
        "- Relative, not a lab spec. It\n"
        "  compares two readings from\n"
        "  the same setup - keep the\n"
        "  distance and angle constant\n"
        "  or the number is noise.\n"
        "- Sub-GHz needs YOUR fob to\n"
        "  transmit. No press, no read.\n"
        "- NFC needs an external reader\n"
        "  field to measure against.\n"
        "- A ' >= ' prefix means the\n"
        "  signal fell below the noise\n"
        "  floor: it is at least that\n"
        "  good, possibly better.\n"
        "- 13.56 MHz only. No 125 kHz.\n"
        "\n"
        "Listen-only. Faraday never\n"
        "transmits.\n"
        "\n"
        "\e#Credits\e#\n"
        "by at0m-b0mb\n"
        "MIT licensed\n"
        "github.com/at0m-b0mb/\n"
        "Faraday-FlipperZero\n");

    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewAbout);
}

bool faraday_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void faraday_scene_about_on_exit(void* context) {
    FaradayApp* app = context;
    widget_reset(app->widget);
}
