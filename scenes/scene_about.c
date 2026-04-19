#include <disney_toolbox_icons.h>

#include "../disney_toolbox_app.h"

void disney_toolbox_app_scene_about_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    Widget* widget = app->widget;

    widget_add_icon_element(widget, 93, 2, &I_qr_code);

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "\e#Disney Toolbox\n");

    furi_string_cat_printf(temp_str, "Version: %s\n", "v0.1");
    furi_string_cat_printf(temp_str, "Developed by: \n    %s\n\n", "Nathaniel Belles");

    furi_string_cat_printf(
        temp_str,
        "Disney Toolbox is a collection of tools for interacting with Disney park technology. "
        "Use the Kyber Crystal Writer to reprogram RFID-based kyber crystals for Savi's Workshop "
        "lightsabers at Galaxy's Edge, choosing from both Series 1 and Series 2 crystals. "
        "The Crystal Checker lets you identify an unknown crystal by reading its data. "
        "The MagicBand+ Beacon feature lets you broadcast BLE advertisement packets that "
        "simulate Disney park interactions, with configurable code types and presets.");

    widget_add_text_scroll_element(widget, 0, 0, 90, 64, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewWidget);
}

bool disney_toolbox_app_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void disney_toolbox_app_scene_about_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    widget_reset(app->widget);
}
