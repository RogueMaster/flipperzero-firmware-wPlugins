#include <applications.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"

static void desktop_settings_scene_keybinds_key_submenu_callback(void* context, uint32_t index) {
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void desktop_settings_scene_keybinds_key_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    submenu_add_item(
        submenu,
        "Up",
        DesktopKeybindKeyUp,
        desktop_settings_scene_keybinds_key_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Down",
        DesktopKeybindKeyDown,
        desktop_settings_scene_keybinds_key_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Right",
        DesktopKeybindKeyRight,
        desktop_settings_scene_keybinds_key_submenu_callback,
        app);

    bool exit_shortcut =
        app->editing_game_keybinds &&
        scene_manager_get_scene_state(app->scene_manager, DesktopSettingsAppSceneKeybindsType) ==
            DesktopKeybindTypeHold;
    submenu_add_lockable_item(
        submenu,
        exit_shortcut ? "Left (Dab Timer)" : "Left",
        DesktopKeybindKeyLeft,
        desktop_settings_scene_keybinds_key_submenu_callback,
        app,
        exit_shortcut,
        "Reserved for\nexiting Game Mode\nthrough Dab Timer.");

    if(app->editing_game_keybinds) {
        submenu_add_item(
            submenu,
            "OK",
            DesktopKeybindKeyOk,
            desktop_settings_scene_keybinds_key_submenu_callback,
            app);
    }
    submenu_set_header(submenu, "Keybind key:");

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, DesktopSettingsAppSceneKeybindsKey));

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewMenu);
}

bool desktop_settings_scene_keybinds_key_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(app->editing_game_keybinds && event.event == DesktopKeybindKeyLeft &&
           scene_manager_get_scene_state(app->scene_manager, DesktopSettingsAppSceneKeybindsType) ==
               DesktopKeybindTypeHold) {
            return true;
        }
        consumed = true;
        scene_manager_set_scene_state(
            app->scene_manager, DesktopSettingsAppSceneKeybindsKey, event.event);
        scene_manager_set_scene_state(
            app->scene_manager,
            DesktopSettingsAppSceneKeybindsActionType,
            DesktopSettingsAppKeybindActionTypeRemoveKeybind);
        scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneKeybindsActionType);
    }
    return consumed;
}

void desktop_settings_scene_keybinds_key_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    submenu_reset(app->submenu);
}
