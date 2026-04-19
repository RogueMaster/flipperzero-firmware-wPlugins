#include <disney_toolbox_icons.h>

#include "../disney_toolbox_app.h"

void disney_toolbox_app_scene_mb_about_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    Widget* widget = app->widget;

    widget_add_icon_element(widget, 93, 2, &I_qr_emcot);

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "\e#MagicBand+\n");

    furi_string_cat_printf(
        temp_str,
        "Broadcast BLE advertisement packets that interact with MagicBand+ wristbands, "
        "triggering light and vibration effects. It is not possible to reliably trigger these "
        "effects to happen immediately because of BLE advertising limitations.\n\n"
        "\e#Code Types\n"
        "Select a code type to configure and broadcast. Each type controls different "
        "LED patterns and behaviors on the MagicBand+.\n\n"
        "\e#Configuration\n"
        "Adjust colors, vibration, timing, and fade settings for each code type. "
        "Press \"Start Broadcast\" to begin transmitting.\n\n"
        "\e#Presets\n"
        "Choose from pre-built beacon configurations for common Disney park interactions.\n\n"
        "\e#More Info\n"
        "Scan the QR code to see where I helped document reverse-engineering of MagicBand+ BLE "
        "codes.");

    widget_add_text_scroll_element(widget, 0, 0, 90, 64, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewWidget);
}

bool disney_toolbox_app_scene_mb_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void disney_toolbox_app_scene_mb_about_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    widget_reset(app->widget);
}
