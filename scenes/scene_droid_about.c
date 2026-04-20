#include "../disney_toolbox_app.h"

void disney_toolbox_app_scene_droid_about_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    Widget* widget = app->widget;

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "\e#Droid Controller\n");

    furi_string_cat_printf(
        temp_str,
        "Broadcast BLE beacons that Galaxy's Edge droids react to.\n\n"
        "\e#Personality Beacons\n"
        "Astromech droids from Droid Depot emit a BLE beacon identifying their installed "
        "personality chip. Other droids react to these beacons.\n\n"
        "\e#Affiliations\n"
        "Each personality chip belongs to an affiliation: Scoundrel, Resistance, or First "
        "Order.\n\n"
        "\e#Location Beacons\n"
        "Location beacons simulate Galaxy's Edge points of interest. Droids react with unique "
        "behaviors depending on the location.\n\n"
        "Interval controls how often the droid reacts. Distance sets the RSSI threshold - lower "
        "values mean the droid must be closer to react.\n\n"
        "\e#Droid Facts\n"
        "Droids wait 2 min between reactions to another droid's beacon. After encountering a "
        "location beacon, a droid will not react to droid beacons for 2 hours. Droids sleep 5 min "
        "after last command or droid beacon encounter.");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewWidget);
}

bool disney_toolbox_app_scene_droid_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void disney_toolbox_app_scene_droid_about_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    widget_reset(app->widget);
}
