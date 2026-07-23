#include "../cerberus_i.h"

void cerberus_scene_about_on_enter(void* context) {
    Cerberus* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "CERBERUS  v%s\n"
        "Sub-GHz RF Watchdog\n"
        "\n"
        "Three heads, three bands.\n"
        "Passively watches 433 /\n"
        "868 / 915 MHz and barks on:\n"
        "  - JAMMING  (held channel)\n"
        "  - FLOOD    (burst storms)\n"
        "  - REPLAY   (repeat captures)\n"
        "\n"
        "Watch screen controls:\n"
        "  OK  - recalibrate floor\n"
        "  Hold OK - arm / silent\n"
        "  Left/Right - meters/scope\n"
        "  Up/Down - pick scope band\n"
        "\n"
        "USE RESPONSIBLY. Only monitor\n"
        "RF in spaces you own or are\n"
        "authorised to test. You are\n"
        "responsible for your local\n"
        "radio regulations.\n"
        "\n"
        "%s\n"
        "by at0m-b0mb",
        CERBERUS_VERSION,
        CERBERUS_GITHUB);

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, CerberusViewWidget);
}

bool cerberus_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cerberus_scene_about_on_exit(void* context) {
    Cerberus* app = context;
    widget_reset(app->widget);
}
