#include "nearby_files_scene.h"
#include "../nearby_files.h"

typedef enum {
    NearbyFilesAddGpsMenuItemSub,
    NearbyFilesAddGpsMenuItemNfc,
    NearbyFilesAddGpsMenuItemRfid,
    NearbyFilesAddGpsMenuItemIbtn,
} NearbyFilesAddGpsMenuItem;

void nearby_files_scene_add_gps_submenu_callback(void* context, uint32_t index) {
    NearbyFilesApp* app = context;

    switch(index) {
    case NearbyFilesAddGpsMenuItemSub:
        view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventAddGpsSub);
        break;
    case NearbyFilesAddGpsMenuItemNfc:
        view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventAddGpsNfc);
        break;
    case NearbyFilesAddGpsMenuItemRfid:
        view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventAddGpsRfid);
        break;
    case NearbyFilesAddGpsMenuItemIbtn:
        view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventAddGpsIbtn);
        break;
    }
}

void nearby_files_scene_add_gps_on_enter(void* context) {
    NearbyFilesApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Add GPS to file");

    submenu_add_item(
        app->submenu,
        "SubGHz file",
        NearbyFilesAddGpsMenuItemSub,
        nearby_files_scene_add_gps_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "NFC file",
        NearbyFilesAddGpsMenuItemNfc,
        nearby_files_scene_add_gps_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "LF RFID file",
        NearbyFilesAddGpsMenuItemRfid,
        nearby_files_scene_add_gps_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "iButton file",
        NearbyFilesAddGpsMenuItemIbtn,
        nearby_files_scene_add_gps_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewSubmenu);
}

bool nearby_files_scene_add_gps_on_event(void* context, SceneManagerEvent event) {
    NearbyFilesApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NearbyFilesCustomEventAddGpsSub:
            nearby_files_add_gps_to_file(app, ".sub", "/ext/subghz");
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        case NearbyFilesCustomEventAddGpsNfc:
            nearby_files_add_gps_to_file(app, ".nfc", "/ext/nfc");
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        case NearbyFilesCustomEventAddGpsRfid:
            nearby_files_add_gps_to_file(app, ".rfid", "/ext/lfrfid");
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        case NearbyFilesCustomEventAddGpsIbtn:
            nearby_files_add_gps_to_file(app, ".ibtn", "/ext/ibutton");
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nearby_files_scene_add_gps_on_exit(void* context) {
    NearbyFilesApp* app = context;
    submenu_reset(app->submenu);
}
