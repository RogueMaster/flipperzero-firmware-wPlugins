#include "../trident_i.h"

typedef enum {
    GpsData,
    GpsWardrive,
    GpsWardriveSta,
} GpsIndex;

static void trident_scene_gps_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_gps_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "GPS / Wardrive");
    submenu_add_item(menu, "GPS Data", GpsData, trident_scene_gps_cb, app);
    submenu_add_item(menu, "Wardrive (AP)", GpsWardrive, trident_scene_gps_cb, app);
    submenu_add_item(menu, "Wardrive (Station)", GpsWardriveSta, trident_scene_gps_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneGps));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_gps_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneGps, event.event);
        consumed = true;
        switch(event.event) {
        case GpsData:
            trident_launch(app, "GPS Data", MARAUDER_CMD_GPS_DATA, false);
            break;
        case GpsWardrive:
            trident_launch(app, "Wardrive AP", MARAUDER_CMD_WARDRIVE, false);
            break;
        case GpsWardriveSta:
            trident_launch(app, "Wardrive STA", MARAUDER_CMD_WARDRIVE_STA, false);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_gps_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
