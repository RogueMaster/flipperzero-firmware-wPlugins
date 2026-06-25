#include "../specter_i.h"

void specter_scene_about_on_enter(void* context) {
    SpecterApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Specter " SPECTER_VERSION);
    widget_add_string_element(
        widget, 64, 14, AlignCenter, AlignTop, FontSecondary, "NFC reader bug-sweep");

    widget_add_text_scroll_element(
        widget,
        0,
        24,
        128,
        40,
        "Specter passively senses active 13.56 MHz\n"
        "NFC reader fields nearby - hidden POS\n"
        "skimmers, rogue door readers, covert\n"
        "loggers - using the onboard NFC chip's\n"
        "external-field detector. It NEVER transmits.\n"
        " \n"
        "Sweep slowly over a payment terminal,\n"
        "door reader or suspicious object. The meter\n"
        "and clicks rise as you near an emitting\n"
        "reader; the bar stays flat when it is clean.\n"
        " \n"
        "OK = reset peak/contacts.\n"
        " \n"
        "Limits: 13.56 MHz (HF) only - it cannot see\n"
        "125 kHz (LF) readers. It senses a reader's\n"
        "carrier, not what it reads. Strength is\n"
        "relative proximity, not a calibrated range.\n"
        " \n"
        "Use only where you are authorised. A\n"
        "defensive, listen-only tool.\n"
        " \n"
        "by at0m-b0mb\n"
        "github.com/at0m-b0mb/Specter-FlipperZero");

    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewAbout);
}

bool specter_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void specter_scene_about_on_exit(void* context) {
    SpecterApp* app = context;
    widget_reset(app->widget);
}
