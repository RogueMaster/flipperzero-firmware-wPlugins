#include "../gemini_app_i.h"

typedef enum {
    GeminiSceneMainMenuIndexSetName,
    GeminiSceneMainMenuIndexConnectNewAP,
    GeminiSceneMainMenuIndexChangeName,
    GeminiSceneMainMenuIndexStartChatting,
    GeminiSceneMainMenuIndexHelp,
} GeminiSceneMainMenuIndex;

typedef enum {
    GeminiSceneMainMenuEventSetName,
    GeminiSceneMainMenuEventConnectNewAP,
    GeminiSceneMainMenuEventConnectChangeName,
    GeminiSceneMainMenuEventStartChatting,
    GeminiSceneMainMenuEventHelp,
} GeminiSceneMainMenuEvent;

static void gemini_scene_main_menu_callback(void* context, uint32_t index) {
    GeminiApp* app = context;
    switch(index) {
    case GeminiSceneMainMenuIndexSetName:
        scene_manager_handle_custom_event(app->scene_manager, GeminiSceneMainMenuEventSetName);
        break;
    case GeminiSceneMainMenuIndexConnectNewAP:
        scene_manager_handle_custom_event(
            app->scene_manager, GeminiSceneMainMenuEventConnectNewAP);
        break;
    case GeminiSceneMainMenuIndexChangeName:
        scene_manager_handle_custom_event(
            app->scene_manager, GeminiSceneMainMenuEventConnectChangeName);
        break;
    case GeminiSceneMainMenuIndexStartChatting:
        scene_manager_handle_custom_event(
            app->scene_manager, GeminiSceneMainMenuEventStartChatting);
        break;
    case GeminiSceneMainMenuIndexHelp:
        scene_manager_handle_custom_event(app->scene_manager, GeminiSceneMainMenuEventHelp);
        break;
    }
}

void gemini_scene_main_menu_on_enter(void* context) {
    GeminiApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Gemini IA");
    submenu_add_item(
        app->submenu,
        "Set your name",
        GeminiSceneMainMenuIndexSetName,
        gemini_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Connect to new AP",
        GeminiSceneMainMenuIndexConnectNewAP,
        gemini_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Change your name",
        GeminiSceneMainMenuIndexChangeName,
        gemini_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Start Chatting",
        GeminiSceneMainMenuIndexStartChatting,
        gemini_scene_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu, "Help", GeminiSceneMainMenuIndexHelp, gemini_scene_main_menu_callback, app);

    uint32_t index = scene_manager_get_scene_state(app->scene_manager, GeminiSceneMainMenu);
    submenu_set_selected_item(app->submenu, index);

    view_dispatcher_switch_to_view(app->view_dispatcher, GeminiViewSubmenu);
}

bool gemini_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    GeminiApp* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case GeminiSceneMainMenuEventSetName:
            scene_manager_next_scene(app->scene_manager, GeminiSceneSetName);
            consumed = true;
            break;
        case GeminiSceneMainMenuEventConnectNewAP:
            // TODO: Replace with correct scene
            scene_manager_next_scene(app->scene_manager, GeminiSceneConnectNewAP);
            consumed = true;
            break;
        case GeminiSceneMainMenuEventConnectChangeName:
            scene_manager_next_scene(app->scene_manager, GeminiSceneChangeName);
            consumed = true;
            break;
        case GeminiSceneMainMenuEventStartChatting:
            scene_manager_next_scene(app->scene_manager, GeminiSceneStartChatting);
            consumed = true;
            break;
        case GeminiSceneMainMenuEventHelp:
            // TODO: Replace with correct scene
            scene_manager_next_scene(app->scene_manager, GeminiSceneHelp);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }

    return consumed;
}

void gemini_scene_main_menu_on_exit(void* context) {
    GeminiApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager, GeminiSceneMainMenu, submenu_get_selected_item(app->submenu));
    submenu_reset(app->submenu);
}
