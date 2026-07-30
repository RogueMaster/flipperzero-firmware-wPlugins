#include "wol_flipper.h"

#include <storage/storage.h>

typedef enum {
    BoardIndexCheck,
    BoardIndexFlash,
    BoardIndexBackup,
    BoardIndexRestore,
    BoardIndexInfo,
} BoardIndex;

static void wol_scene_board_callback(void* context, uint32_t index) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void wol_scene_board_on_enter(void* context) {
    WolApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ESP board");
    submenu_add_item(
        app->submenu, "Firmware check", BoardIndexCheck, wol_scene_board_callback, app);
    submenu_add_item(
        app->submenu, "Flash WoL firmware", BoardIndexFlash, wol_scene_board_callback, app);
    submenu_add_item(
        app->submenu, "Backup ESP flash", BoardIndexBackup, wol_scene_board_callback, app);
    submenu_add_item(
        app->submenu, "Restore backup", BoardIndexRestore, wol_scene_board_callback, app);
    submenu_add_item(app->submenu, "Board info", BoardIndexInfo, wol_scene_board_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, WolSceneBoard));
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewSubmenu);
}

/** Ask for a dump to restore. Returns false when the user backs out. */
static bool wol_scene_board_pick_backup(WolApp* app) {
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".bin", NULL);
    options.base_path = WOL_BACKUP_DIR;
    options.hide_ext = false;

    furi_string_set(app->flasher_path, WOL_BACKUP_DIR);
    return dialog_file_browser_show(app->dialogs, app->flasher_path, app->flasher_path, &options);
}

bool wol_scene_board_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event > BoardIndexInfo) return false;

    scene_manager_set_scene_state(app->scene_manager, WolSceneBoard, event.event);

    switch(event.event) {
    case BoardIndexCheck:
        // talks to the running firmware, not the bootloader: no BOOT+RESET
        app->wake_op = WolWakeOpPing;
        scene_manager_next_scene(app->scene_manager, WolSceneSend);
        return true;

    case BoardIndexFlash:
        app->flasher_op = WolFlasherOpFlashFirmware;
        break;
    case BoardIndexBackup:
        app->flasher_op = WolFlasherOpBackup;
        break;
    case BoardIndexRestore:
        if(!wol_scene_board_pick_backup(app)) return true;
        app->flasher_op = WolFlasherOpRestore;
        break;
    case BoardIndexInfo:
        app->flasher_op = WolFlasherOpInfo;
        break;
    default:
        return false;
    }

    scene_manager_next_scene(app->scene_manager, WolSceneBootMode);
    return true;
}

void wol_scene_board_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}
