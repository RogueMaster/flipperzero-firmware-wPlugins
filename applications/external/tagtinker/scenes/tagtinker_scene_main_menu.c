/*
 * Main Menu
 */

#include "../tagtinker_app.h"

enum {
    MainMenuBroadcast,
    MainMenuTargetESL,
    MainMenuSettings,
    MainMenuAndroid,
    MainMenuAbout,
};

static void main_menu_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tagtinker_scene_main_menu_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, TAGTINKER_DISPLAY_NAME " v" TAGTINKER_VERSION);

    submenu_add_item(app->submenu, "Broadcast Payloads", MainMenuBroadcast, main_menu_cb, app);
    submenu_add_item(app->submenu, "Targeted Payloads", MainMenuTargetESL, main_menu_cb, app);
    submenu_add_item(app->submenu, "Phone Sync", MainMenuAndroid, main_menu_cb, app);
    submenu_add_item(app->submenu, "Settings", MainMenuSettings, main_menu_cb, app);
    submenu_add_item(app->submenu, "About", MainMenuAbout, main_menu_cb, app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, TagTinkerSceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewSubmenu);
}

bool tagtinker_scene_main_menu_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, TagTinkerSceneMainMenu, event.event);

    switch(event.event) {
    case MainMenuBroadcast:
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneBroadcastMenu);
        return true;
    case MainMenuTargetESL:
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTargetMenu);
        return true;
    case MainMenuSettings:
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneSettings);
        return true;
    case MainMenuAndroid:
        /* state=1 tells About scene to show Android teaser */
        scene_manager_set_scene_state(app->scene_manager, TagTinkerSceneAbout, 1);
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneAbout);
        return true;
    case MainMenuAbout:
        scene_manager_set_scene_state(app->scene_manager, TagTinkerSceneAbout, 0);
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneAbout);
        return true;
    }
    return false;
}

void tagtinker_scene_main_menu_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    submenu_reset(app->submenu);
}
