#include "../k2_rfid_app.h"
#include "k2_scene.h"

typedef enum {
    SubmenuIndexScan,
    SubmenuIndexWrite,
    SubmenuIndexEmulate,
    SubmenuIndexConfig,
    SubmenuIndexSave,
    SubmenuIndexSavedSpools,
    SubmenuIndexFormat,
    SubmenuIndexAbout,
    SubmenuIndexExit,
} SubmenuIndex;

static void k2_scene_main_menu_callback(void* context, uint32_t index) {
    K2RfidApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void k2_scene_main_menu_on_enter(void* context) {
    K2RfidApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Creality CFS RFID");

    submenu_add_item(submenu, "Scan Spool Tag", SubmenuIndexScan, k2_scene_main_menu_callback, app);
    submenu_add_item(submenu, "Write Spool Tag", SubmenuIndexWrite, k2_scene_main_menu_callback, app);
    submenu_add_item(submenu, "Emulate Spool", SubmenuIndexEmulate, k2_scene_main_menu_callback, app);
    submenu_add_item(submenu, "Spool Settings", SubmenuIndexConfig, k2_scene_main_menu_callback, app);
    submenu_add_item(submenu, "Save to .NFC", SubmenuIndexSave, k2_scene_main_menu_callback, app);
    submenu_add_item(submenu, "Saved Spools (.nfc)", SubmenuIndexSavedSpools, k2_scene_main_menu_callback, app);
    submenu_add_item(submenu, "Format / Erase Tag", SubmenuIndexFormat, k2_scene_main_menu_callback, app);
    submenu_add_item(submenu, "About", SubmenuIndexAbout, k2_scene_main_menu_callback, app);
    submenu_add_item(submenu, "Exit", SubmenuIndexExit, k2_scene_main_menu_callback, app);

    submenu_set_selected_item(submenu, scene_manager_get_scene_state(app->scene_manager, K2SceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewSubmenu);
}

bool k2_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    K2RfidApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    } else if (event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, K2SceneMainMenu, event.event);
        consumed = true;
        switch (event.event) {
        case SubmenuIndexScan:
            scene_manager_next_scene(app->scene_manager, K2SceneScan);
            break;
        case SubmenuIndexWrite:
            scene_manager_next_scene(app->scene_manager, K2SceneWrite);
            break;
        case SubmenuIndexEmulate:
            scene_manager_next_scene(app->scene_manager, K2SceneEmulate);
            break;
        case SubmenuIndexConfig:
            scene_manager_next_scene(app->scene_manager, K2SceneConfig);
            break;
        case SubmenuIndexSave:
            scene_manager_next_scene(app->scene_manager, K2SceneSave);
            break;
        case SubmenuIndexSavedSpools:
            scene_manager_next_scene(app->scene_manager, K2SceneSavedSpools);
            break;
        case SubmenuIndexFormat:
            scene_manager_next_scene(app->scene_manager, K2SceneFormat);
            break;
        case SubmenuIndexAbout:
            scene_manager_next_scene(app->scene_manager, K2SceneAbout);
            break;
        case SubmenuIndexExit:
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void k2_scene_main_menu_on_exit(void* context) {
    K2RfidApp* app = context;
    submenu_reset(app->submenu);
}
