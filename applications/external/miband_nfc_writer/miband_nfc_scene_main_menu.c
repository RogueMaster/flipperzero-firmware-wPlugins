/**
 * @file miband_nfc_scene_main_menu.c
 * @brief Main menu scene - Updated with new features
 */

#include "miband_nfc_i.h"

void miband_nfc_app_submenu_callback(void* context, uint32_t index) {
    MiBandNfcApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void miband_nfc_scene_main_menu_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Mi Band NFC");

    // Quick operations
    submenu_add_item(
        app->submenu,
        "Quick UID Check",
        SubmenuIndexQuickUidCheck,
        miband_nfc_app_submenu_callback,
        app);

    // Emulation functions
    submenu_add_item(
        app->submenu,
        "Emulate Magic Card",
        SubmenuIndexEmulateNfcMagic,
        miband_nfc_app_submenu_callback,
        app);

    // Writing functions (now with auto-backup)
    submenu_add_item(
        app->submenu,
        "Write Original Data",
        SubmenuIndexWriteOriginalData,
        miband_nfc_app_submenu_callback,
        app);

    // Save magic dump
    submenu_add_item(
        app->submenu,
        "Save Magic Dump",
        SubmenuIndexSaveMagic,
        miband_nfc_app_submenu_callback,
        app);

    // Verification
    submenu_add_item(
        app->submenu, "Verify Write", SubmenuIndexVerify, miband_nfc_app_submenu_callback, app);

    // Settings
    submenu_add_item(
        app->submenu, "Settings >", SubmenuIndexSettings, miband_nfc_app_submenu_callback, app);

    // About
    submenu_add_item(
        app->submenu, "About", SubmenuIndexAbout, miband_nfc_app_submenu_callback, app);

    submenu_set_selected_item(app->submenu, app->last_selected_submenu_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdMainMenu);
}

bool miband_nfc_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        app->last_selected_submenu_index = event.event;

        switch(event.event) {
        case SubmenuIndexQuickUidCheck:
            // NEW: Quick UID check
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneUidCheck);
            consumed = true;
            break;

        case SubmenuIndexEmulateNfcMagic:
            app->current_operation = OperationTypeEmulateMagic;
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneFileSelect);
            consumed = true;
            break;

        case SubmenuIndexWriteOriginalData:
            // NEW: Route through backup scene first
            app->current_operation = OperationTypeWriteOriginal;
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneFileSelect);
            consumed = true;
            break;

        case SubmenuIndexSaveMagic:
            app->current_operation = OperationTypeSaveMagic;
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneFileSelect);
            consumed = true;
            break;

        case SubmenuIndexVerify:
            app->current_operation = OperationTypeVerify;
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneFileSelect);
            consumed = true;
            break;

        case SubmenuIndexSettings:
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneSettings);
            consumed = true;
            break;

        case SubmenuIndexAbout:
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneAbout);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void miband_nfc_scene_main_menu_on_exit(void* context) {
    MiBandNfcApp* app = context;
    submenu_reset(app->submenu);
}
