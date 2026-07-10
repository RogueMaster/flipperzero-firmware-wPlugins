#include "../warden_i.h"

void warden_scene_about_on_enter(void* context) {
    WardenApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    FuriString* s = furi_string_alloc();
    furi_string_cat_str(s, "\e#Warden " WARDEN_VERSION "\n");
    furi_string_cat_str(s, "Access-card grader\n\n");
    furi_string_cat_str(
        s,
        "Hold your own badge to the\n"
        "Flipper's back. Warden reads the\n"
        "technology and hands back a\n"
        "plain-English security grade.\n\n");
    furi_string_cat_str(s, "\e#How it grades\n");
    furi_string_cat_str(
        s,
        "The tech decides the crypto:\n"
        "[x] Classic - Crypto1 is broken\n"
        "[!] Ultralight/UID - cloneable\n"
        "[!] ISO15693 - depends on system\n"
        "[+] DESFire/FeliCa - modern AES\n\n");
    furi_string_cat_str(
        s,
        "Read-only. Warden never writes,\n"
        "never cracks keys, never leaves\n"
        "a trace on the card.\n\n");
    furi_string_cat_str(s, "\e#Ethics\n");
    furi_string_cat_str(
        s,
        "Grade cards you own or are\n"
        "authorised to test. Know your\n"
        "own doors before someone else\n"
        "does.\n\n");
    furi_string_cat_str(s, "by at0m-b0mb\n");
    furi_string_cat_str(s, "github.com/at0m-b0mb/\nWarden-FlipperZero\n");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    furi_string_free(s);

    view_dispatcher_switch_to_view(app->view_dispatcher, WardenViewWidget);
}

bool warden_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void warden_scene_about_on_exit(void* context) {
    WardenApp* app = context;
    widget_reset(app->widget);
}
