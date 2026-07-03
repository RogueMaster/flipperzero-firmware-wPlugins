#include "../disn3y_toolbox_app.h"

void disn3y_toolbox_app_scene_kyber_about_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    Widget* widget = app->widget;

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "\e#Kyber Crystals\n");

    furi_string_cat_printf(
        temp_str,
        "Write kyber crystal identities to 125kHz RFID tags for use with Savi's Workshop "
        "lightsabers and holocrons at Star Wars: Galaxy's Edge. Two series of crystals exist "
        "with some differences in voice lines and effects.\n\n"
        "\e#Series 1 Writer\n"
        "Select from 19 original kyber crystal colors and write them to a compatible RFID tag. "
        "Each crystal has a unique color, Jedi and Sith voice, and "
        "wayfinder location. Compatible with Series 2 hilts and holocrons.\n\n"
        "\e#Series 2 Writer\n"
        "Select from 18 newer crystals with additional voice lines and effects. "
        "Series 2 crystals include a fallback to a Series 1 identity for older hilts. "
        "This is currently in beta with partial functionality implemented. Should be "
        "compatible with Series 1 hilts (although with varying functionality).\n\n"
        "\e#Crystal Checker\n"
        "Place a kyber crystal on the Flipper's RFID reader to identify which crystal it is "
        "and view its properties.");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewWidget);
}

bool disn3y_toolbox_app_scene_kyber_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void disn3y_toolbox_app_scene_kyber_about_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    widget_reset(app->widget);
}
