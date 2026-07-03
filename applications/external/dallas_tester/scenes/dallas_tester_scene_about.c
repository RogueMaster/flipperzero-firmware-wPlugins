#include "../dallas_tester_i.h"

void dallas_tester_scene_about_on_enter(void* context) {
    DallasTester* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Dallas Tester");

    widget_add_text_scroll_element(
        widget,
        0,
        15,
        128,
        49,
        "Read-only tester for Dallas\n"
        "iButton keys and blanks.\n"
        " \n"
        "FAM - family code (01=DS1990)\n"
        "CRC - ROM checksum ok/bad\n"
        "tPDL - presence-low time us\n"
        "  (median; lo-hi; sdN=jitter)\n"
        "  ~100-160us healthy; short\n"
        "  is rejected by intercoms\n"
        "  (e.g. Vizit)\n"
        "tPDH - presence-high time us\n"
        "n - valid reads per batch\n"
        " \n"
        "Never writes to the key.\n"
        " \n"
        "Author: mishamyte\n"
        "github.com/mishamyte/\n"
        "flipper-dallas-tester\n"
        "Version: 0.1");

    view_dispatcher_switch_to_view(app->view_dispatcher, DallasTesterViewWidget);
}

bool dallas_tester_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void dallas_tester_scene_about_on_exit(void* context) {
    DallasTester* app = context;
    widget_reset(app->widget);
}
