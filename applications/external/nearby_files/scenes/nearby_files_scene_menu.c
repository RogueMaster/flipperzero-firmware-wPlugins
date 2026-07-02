#include "nearby_files_scene.h"
#include "../nearby_files.h"

typedef enum {
    NearbyFilesMenuItemRefreshList,
    NearbyFilesMenuItemAddGpsPosition,
    NearbyFilesMenuItemGpsBaudrate,
    NearbyFilesMenuItemAbout,
    NearbyFilesMenuItemExit,
} NearbyFilesMenuItem;

void nearby_files_scene_menu_submenu_callback(void* context, uint32_t index) {
    NearbyFilesApp* app = context;

    switch(index) {
    case NearbyFilesMenuItemRefreshList:
        view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventRefreshList);
        break;
    case NearbyFilesMenuItemAddGpsPosition:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NearbyFilesCustomEventAddGpsPosition);
        break;
    case NearbyFilesMenuItemGpsBaudrate:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NearbyFilesCustomEventGpsBaudrateMenu);
        break;
    case NearbyFilesMenuItemAbout:
        view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventAbout);
        break;
    case NearbyFilesMenuItemExit:
        view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventExit);
        break;
    }
}

void nearby_files_scene_menu_on_enter(void* context) {
    NearbyFilesApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, NEARBY_FILES_APP_NAME " v" NEARBY_FILES_VERSION);

    submenu_add_item(
        app->submenu,
        "Refresh List",
        NearbyFilesMenuItemRefreshList,
        nearby_files_scene_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Add GPS to file",
        NearbyFilesMenuItemAddGpsPosition,
        nearby_files_scene_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "GPS Baudrate",
        NearbyFilesMenuItemGpsBaudrate,
        nearby_files_scene_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "About",
        NearbyFilesMenuItemAbout,
        nearby_files_scene_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Exit",
        NearbyFilesMenuItemExit,
        nearby_files_scene_menu_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewSubmenu);
}

bool nearby_files_scene_menu_on_event(void* context, SceneManagerEvent event) {
    NearbyFilesApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NearbyFilesCustomEventRefreshList:
            // Clear existing files and go to start scene to wait for GPS
            nearby_files_clear_files(app);
            scene_manager_next_scene(app->scene_manager, NearbyFilesSceneStart);
            consumed = true;
            break;
        case NearbyFilesCustomEventAddGpsPosition:
            scene_manager_next_scene(app->scene_manager, NearbyFilesSceneAddGps);
            consumed = true;
            break;
        case NearbyFilesCustomEventGpsBaudrateMenu:
            scene_manager_next_scene(app->scene_manager, NearbyFilesSceneGpsBaudrate);
            consumed = true;
            break;
        case NearbyFilesCustomEventAbout:
            // Go to about scene
            scene_manager_next_scene(app->scene_manager, NearbyFilesSceneAbout);
            consumed = true;
            break;
        case NearbyFilesCustomEventExit:
            // Exit the app when Exit menu item is selected
            view_dispatcher_stop(app->view_dispatcher);
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Exit the app when back button is pressed in menu
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void nearby_files_scene_menu_on_exit(void* context) {
    NearbyFilesApp* app = context;
    submenu_reset(app->submenu);
}
