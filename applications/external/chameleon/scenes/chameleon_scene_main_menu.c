#include "../chameleon_app_i.h"

typedef enum {
    SubmenuIndexConnect,
    SubmenuIndexSlots,
    SubmenuIndexReadTag,
    SubmenuIndexWriteTag,
    SubmenuIndexDiagnostic,
    SubmenuIndexAbout,
} SubmenuIndex;

static void chameleon_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void chameleon_scene_main_menu_on_enter(void* context) {
    ChameleonApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);

    submenu_add_item(
        submenu,
        "Connect Device",
        SubmenuIndexConnect,
        chameleon_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Manage Slots",
        SubmenuIndexSlots,
        chameleon_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu, "Read Tag", SubmenuIndexReadTag, chameleon_scene_main_menu_submenu_callback, app);

    submenu_add_item(
        submenu,
        "Write to Chameleon",
        SubmenuIndexWriteTag,
        chameleon_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Diagnostic",
        SubmenuIndexDiagnostic,
        chameleon_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, chameleon_scene_main_menu_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
}

bool chameleon_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Exit the app when back is pressed on main menu
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexConnect:
            scene_manager_next_scene(app->scene_manager, ChameleonSceneConnectionType);
            consumed = true;
            break;
        case SubmenuIndexSlots:
            if(app->connection_status == ChameleonStatusConnected) {
                scene_manager_next_scene(app->scene_manager, ChameleonSceneSlotList);
            } else {
                // Show error popup
                popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup, "Not connected\nto device", 64, 32, AlignCenter, AlignCenter);
                view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);
                furi_delay_ms(1500);
                view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
            }
            consumed = true;
            break;
        case SubmenuIndexReadTag:
            if(app->connection_status == ChameleonStatusConnected) {
                scene_manager_next_scene(app->scene_manager, ChameleonSceneTagRead);
            } else {
                popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup, "Not connected\nto device", 64, 32, AlignCenter, AlignCenter);
                view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);
                furi_delay_ms(1500);
                view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
            }
            consumed = true;
            break;
        case SubmenuIndexWriteTag:
            if(app->connection_status == ChameleonStatusConnected) {
                scene_manager_next_scene(app->scene_manager, ChameleonSceneTagWrite);
            } else {
                popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup, "Not connected\nto device", 64, 32, AlignCenter, AlignCenter);
                view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);
                furi_delay_ms(1500);
                view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
            }
            consumed = true;
            break;
        case SubmenuIndexDiagnostic:
            if(app->connection_status == ChameleonStatusConnected) {
                scene_manager_next_scene(app->scene_manager, ChameleonSceneDiagnostic);
            } else {
                popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup, "Not connected\nto device", 64, 32, AlignCenter, AlignCenter);
                view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);
                furi_delay_ms(1500);
                view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
            }
            consumed = true;
            break;
        case SubmenuIndexAbout:
            scene_manager_next_scene(app->scene_manager, ChameleonSceneAbout);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void chameleon_scene_main_menu_on_exit(void* context) {
    ChameleonApp* app = context;
    submenu_reset(app->submenu);
}
