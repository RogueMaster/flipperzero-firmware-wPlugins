#include "../application.h"
#include "scenes.h"

typedef enum {
    SceneMainMenuItemSetFolder,
    SceneMainMenuItemSetMode,
    SceneMainMenuItemSetSpecial1,
    SceneMainMenuItemSetSpecial2,
    SceneMainMenuItemViewCard,
    SceneMainMenuItemReadCard,
    SceneMainMenuItemWriteCard,
    SceneMainMenuItemRapidWrite,
    SceneMainMenuItemAbout,
} SceneMainMenuItem;

static void tonuino_scene_main_menu_callback(void* context, uint32_t index) {
    TonuinoApp* app = context;
    // Save selected index before navigating away
    scene_manager_set_scene_state(app->scene_manager, TonuinoSceneMainMenu, index);
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void tonuino_scene_main_menu_on_enter(void* context) {
    TonuinoApp* app = context;

    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu,
        "Set Folder",
        SceneMainMenuItemSetFolder,
        tonuino_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu, "Set Mode", SceneMainMenuItemSetMode, tonuino_scene_main_menu_callback, app);
    submenu_add_item(
        app->submenu,
        "Set Special 1",
        SceneMainMenuItemSetSpecial1,
        tonuino_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Set Special 2",
        SceneMainMenuItemSetSpecial2,
        tonuino_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "View Card",
        SceneMainMenuItemViewCard,
        tonuino_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Read Card",
        SceneMainMenuItemReadCard,
        tonuino_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Write Card",
        SceneMainMenuItemWriteCard,
        tonuino_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Rapid Write",
        SceneMainMenuItemRapidWrite,
        tonuino_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu, "About", SceneMainMenuItemAbout, tonuino_scene_main_menu_callback, app);

    // Restore previously selected item
    uint32_t selected_item =
        scene_manager_get_scene_state(app->scene_manager, TonuinoSceneMainMenu);
    submenu_set_selected_item(app->submenu, selected_item);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewSubmenu);
}

bool tonuino_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    TonuinoApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SceneMainMenuItemSetFolder:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneSetFolder);
            return true;
        case SceneMainMenuItemSetMode:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneSetMode);
            return true;
        case SceneMainMenuItemSetSpecial1:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneSetSpecial1);
            return true;
        case SceneMainMenuItemSetSpecial2:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneSetSpecial2);
            return true;
        case SceneMainMenuItemViewCard:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneViewCard);
            return true;
        case SceneMainMenuItemReadCard:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneReadCardWaiting);
            return true;
        case SceneMainMenuItemWriteCard:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneWriteCardWaiting);
            return true;
        case SceneMainMenuItemRapidWrite:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneRapidWrite);
            return true;
        case SceneMainMenuItemAbout:
            scene_manager_next_scene(app->scene_manager, TonuinoSceneAbout);
            return true;
        }
    }

    return false;
}

void tonuino_scene_main_menu_on_exit(void* context) {
    TonuinoApp* app = context;
    submenu_reset(app->submenu);
}
