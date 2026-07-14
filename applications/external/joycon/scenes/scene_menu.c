#include "../switch_controller_app.h"

typedef enum {
    SubmenuIndexController,
    SubmenuIndexRecordMacro,
    SubmenuIndexPlayMacro,
    SubmenuIndexManageMacros,
} SubmenuIndex;

static void switch_controller_scene_menu_submenu_callback(void* context, uint32_t index) {
    SwitchControllerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void switch_controller_scene_on_enter_menu(void* context) {
    SwitchControllerApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu,
        "Manual Controller",
        SubmenuIndexController,
        switch_controller_scene_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Record Macro",
        SubmenuIndexRecordMacro,
        switch_controller_scene_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Play Macro",
        SubmenuIndexPlayMacro,
        switch_controller_scene_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Manage Macros",
        SubmenuIndexManageMacros,
        switch_controller_scene_menu_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, SwitchControllerSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, SwitchControllerViewSubmenu);
}

bool switch_controller_scene_on_event_menu(void* context, SceneManagerEvent event) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, SwitchControllerSceneMenu, event.event);
        switch(event.event) {
        case SubmenuIndexController:
            scene_manager_next_scene(app->scene_manager, SwitchControllerSceneController);
            consumed = true;
            break;
        case SubmenuIndexRecordMacro:
            scene_manager_next_scene(app->scene_manager, SwitchControllerSceneMacroName);
            consumed = true;
            break;
        case SubmenuIndexPlayMacro:
            scene_manager_next_scene(app->scene_manager, SwitchControllerSceneMacroList);
            consumed = true;
            break;
        case SubmenuIndexManageMacros:
            scene_manager_next_scene(app->scene_manager, SwitchControllerSceneMacroList);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void switch_controller_scene_on_exit_menu(void* context) {
    SwitchControllerApp* app = context;
    submenu_reset(app->submenu);
}
