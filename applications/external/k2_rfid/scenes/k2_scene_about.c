#include "../k2_rfid_app.h"
#include "k2_scene.h"

void k2_scene_about_on_enter(void* context) {
    K2RfidApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    FuriString* str = furi_string_alloc();
    furi_string_printf(
        str,
        "Creality CFS RFID v1.0\n"
        "Ported from K2-RFID\n"
        "(DnG-Crafts) by mitchsurp\n\n"
        "Features:\n"
        "- Scan & Decrypt Spools\n"
        "- Write Blank Tags\n"
        "- Emulate Spools to CFS\n"
        "- Save/Load .NFC on SD\n"
        "- 66 Filament Database\n"
        "- Tag Format/Blanking");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(str));
    furi_string_free(str);

    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewWidget);
}

bool k2_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void k2_scene_about_on_exit(void* context) {
    K2RfidApp* app = context;
    widget_reset(app->widget);
}
