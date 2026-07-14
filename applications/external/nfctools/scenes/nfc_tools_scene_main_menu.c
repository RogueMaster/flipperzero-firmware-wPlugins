#include "../include/nfc_tools_i.h"

typedef enum {
    NfcToolsMainMenuItemReadTag,
    NfcToolsMainMenuItemWriteNdef,
    NfcToolsMainMenuItemAutre,
    NfcToolsMainMenuItemAbout,
} NfcToolsMainMenuItem;

static void nfc_tools_scene_main_menu_callback(void* context, uint32_t index) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_tools_scene_main_menu_on_enter(void* context) {
    NfcToolsApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, NTS_APP_NAME);
    submenu_add_item(
        submenu,
        NTS_MAIN_READ,
        NfcToolsMainMenuItemReadTag,
        nfc_tools_scene_main_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_MAIN_WRITE,
        NfcToolsMainMenuItemWriteNdef,
        nfc_tools_scene_main_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_MAIN_OTHER,
        NfcToolsMainMenuItemAutre,
        nfc_tools_scene_main_menu_callback,
        app);
    submenu_add_item(
        submenu,
        NTS_MAIN_ABOUT,
        NfcToolsMainMenuItemAbout,
        nfc_tools_scene_main_menu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewMainMenu);
}

bool nfc_tools_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NfcToolsMainMenuItemReadTag:
            app->scan_destination = NfcToolsSceneIdTagInfo;
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdReadTag);
            consumed = true;
            break;
        case NfcToolsMainMenuItemWriteNdef:
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteMenu);
            consumed = true;
            break;
        case NfcToolsMainMenuItemAutre:
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdOtherMenu);
            consumed = true;
            break;
        case NfcToolsMainMenuItemAbout:
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void nfc_tools_scene_main_menu_on_exit(void* context) {
    NfcToolsApp* app = context;
    submenu_reset(app->submenu);
}
