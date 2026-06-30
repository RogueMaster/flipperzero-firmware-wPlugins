#include "../flipcheckers.h"

enum SubmenuIndex {
    SubmenuIndexScene1New = 10,
    SubmenuIndexScene1Resume,
    SubmenuIndexScene1Import,
    SubmenuIndexSettings,
    SubmenuIndexAbout,
};

void flipcheckers_scene_menu_submenu_callback(void* context, uint32_t index) {
    FlipCheckers* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipcheckers_scene_menu_on_enter(void* context) {
    FlipCheckers* app = context;

    submenu_add_item(
        app->submenu,
        "New Game",
        SubmenuIndexScene1New,
        flipcheckers_scene_menu_submenu_callback,
        app);

    if(app->import_game == 1) {
        submenu_add_item(
            app->submenu,
            "Resume Game",
            SubmenuIndexScene1Resume,
            flipcheckers_scene_menu_submenu_callback,
            app);
    }

    // submenu_add_item(
    //     app->submenu,
    //     "Import Game",
    //     SubmenuIndexScene1Import,
    //     flipcheckers_scene_menu_submenu_callback,
    //     app);

    submenu_add_item(
        app->submenu, "Settings", SubmenuIndexSettings, flipcheckers_scene_menu_submenu_callback, app);

    submenu_add_item(
        app->submenu, "About", SubmenuIndexAbout, flipcheckers_scene_menu_submenu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, FlipCheckersSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCheckersViewIdMenu);
}

bool flipcheckers_scene_menu_on_event(void* context, SceneManagerEvent event) {
    FlipCheckers* app = context;
    //UNUSED(app);
    if(event.type == SceneManagerEventTypeBack) {
        //exit app
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexScene1New) {
            app->import_game = 0;
            scene_manager_set_scene_state(
                app->scene_manager, FlipCheckersSceneMenu, SubmenuIndexScene1New);
            scene_manager_next_scene(app->scene_manager, FlipCheckersSceneScene_1);
            return true;
        }
        if(event.event == SubmenuIndexScene1Resume) {
            app->import_game = 1;
            scene_manager_set_scene_state(
                app->scene_manager, FlipCheckersSceneMenu, SubmenuIndexScene1Resume);
            scene_manager_next_scene(app->scene_manager, FlipCheckersSceneScene_1);
            return true;
        } else if(event.event == SubmenuIndexScene1Import) {
            app->import_game = 1;
            app->input_state = FlipCheckersTextInputGame;
            text_input_set_header_text(app->text_input, "Enter board FEN");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipCheckersViewIdTextInput);
            return true;
        } else if(event.event == SubmenuIndexSettings) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipCheckersSceneMenu, SubmenuIndexSettings);
            scene_manager_next_scene(app->scene_manager, FlipCheckersSceneSettings);
            return true;
        } else if(event.event == SubmenuIndexAbout) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipCheckersSceneMenu, SubmenuIndexAbout);
            scene_manager_next_scene(app->scene_manager, FlipCheckersSceneAbout);
            return true;
        }
    }
    return false;
}

void flipcheckers_scene_menu_on_exit(void* context) {
    FlipCheckers* app = context;
    submenu_reset(app->submenu);
}
