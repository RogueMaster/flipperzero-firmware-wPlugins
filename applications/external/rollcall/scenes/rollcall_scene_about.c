#include "../rollcall_i.h"

void rollcall_scene_about_on_enter(void* context) {
    RollCallApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    FuriString* s = furi_string_alloc();
    furi_string_cat_str(s, "\e#RollCall " ROLLCALL_VERSION "\n");
    furi_string_cat_str(s, "Rolling-code health check\n\n");
    furi_string_cat_str(
        s,
        "Press your own remote a few times.\n"
        "RollCall decodes each press and\n"
        "tells you if the code resists a\n"
        "replay attack - or not.\n\n");

    furi_string_cat_str(s, "\e#Rolling vs fixed\n");
    furi_string_cat_str(
        s,
        "[+] Rolling code: a fresh parcel\n"
        "    every press (KeeLoq, Nice\n"
        "    Flor-S, CAME Atomo...). A\n"
        "    recorded press is useless -\n"
        "    the code already moved on.\n\n"
        "[x] Fixed code: the same parcel\n"
        "    forever (Princeton, CAME,\n"
        "    Nice FLO, Holtek...). Record\n"
        "    once, replay any time.\n\n");

    furi_string_cat_str(s, "\e#How to use\n");
    furi_string_cat_str(
        s,
        "1. Pick the band/modulation in\n"
        "   Settings (315 US / 433.92 EU,\n"
        "   AM covers most fobs).\n"
        "2. Run the check and press your\n"
        "   remote, pausing ~1s between\n"
        "   presses.\n"
        "3. Read the grade. Open Details\n"
        "   to see each press change (or\n"
        "   not).\n\n");

    furi_string_cat_str(s, "\e#Ethics\n");
    furi_string_cat_str(
        s,
        "Test only what you own or are\n"
        "authorised to check. RollCall is\n"
        "listen-only: it never transmits,\n"
        "replays or clones a thing.\n\n");

    furi_string_cat_str(s, "by at0m-b0mb\n");
    furi_string_cat_str(s, "github.com/at0m-b0mb/\nRollCall-FlipperZero\n");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    furi_string_free(s);

    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewWidget);
}

bool rollcall_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rollcall_scene_about_on_exit(void* context) {
    RollCallApp* app = context;
    widget_reset(app->widget);
}
