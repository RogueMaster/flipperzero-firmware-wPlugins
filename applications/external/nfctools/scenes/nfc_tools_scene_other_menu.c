#include "../include/nfc_tools_i.h"

typedef enum {
    OtherMenuItemSectorAnalysis,
    OtherMenuItemEffacerTag,
    OtherMenuItemFormatTag,
    OtherMenuItemSetPassword,
    OtherMenuItemRemovePassword,
    OtherMenuItemLockTag,
    OtherMenuItemNfcCommands,
} OtherMenuItem;

static void nfc_tools_scene_other_menu_callback(void* context, uint32_t index) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_tools_scene_other_menu_on_enter(void* context) {
    NfcToolsApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, NTS_HEADER_OTHER);
    submenu_add_item(
        submenu,
        NTS_OTHER_ERASE_TAG,
        OtherMenuItemEffacerTag,
        nfc_tools_scene_other_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_OTHER_LOCK_TAG,
        OtherMenuItemLockTag,
        nfc_tools_scene_other_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_OTHER_READ_MEMORY,
        OtherMenuItemSectorAnalysis,
        nfc_tools_scene_other_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_OTHER_FORMAT_MEMORY,
        OtherMenuItemFormatTag,
        nfc_tools_scene_other_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_OTHER_SET_PASSWORD,
        OtherMenuItemSetPassword,
        nfc_tools_scene_other_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_OTHER_REMOVE_PASSWORD,
        OtherMenuItemRemovePassword,
        nfc_tools_scene_other_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_OTHER_NFC_COMMANDS,
        OtherMenuItemNfcCommands,
        nfc_tools_scene_other_menu_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewMainMenu);
}

bool nfc_tools_scene_other_menu_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case OtherMenuItemSectorAnalysis:
            app->scan_destination = NfcToolsSceneIdSectorAnalysis;
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdReadTag);
            consumed = true;
            break;
        case OtherMenuItemEffacerTag:
            app->ndef_type = NdefTypeEmpty;
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteScan);
            consumed = true;
            break;
        case OtherMenuItemFormatTag:
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdFormatTag);
            consumed = true;
            break;
        case OtherMenuItemSetPassword:
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdSetPasswordInput);
            consumed = true;
            break;
        case OtherMenuItemRemovePassword:
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdRemovePasswordInput);
            consumed = true;
            break;
        case OtherMenuItemLockTag:
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdLockTag);
            consumed = true;
            break;
        case OtherMenuItemNfcCommands:
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdNfcCommandsInput);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void nfc_tools_scene_other_menu_on_exit(void* context) {
    NfcToolsApp* app = context;
    submenu_reset(app->submenu);
}
