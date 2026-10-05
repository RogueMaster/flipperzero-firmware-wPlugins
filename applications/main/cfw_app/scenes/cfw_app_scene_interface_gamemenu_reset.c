#include "../cfw_app.h"

static void gamemenu_reset_callback(DialogExResult result, void* context) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void cfw_app_scene_interface_gamemenu_reset_on_enter(void* context) {
    CFWApp* app = context;
    DialogEx* dialog = app->dialog_ex;
    bool delete_all =
        scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterfaceGamemenuReset);
    dialog_ex_set_header(
        dialog,
        delete_all ? "Clear Game Menu?" : "Rebuild Game Menu?",
        64,
        10,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_text(
        dialog,
        delete_all ? "Remove all menu entries?\nInstalled apps stay on SD." :
                     "Restore all installed games?",
        64,
        32,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(dialog, "Cancel");
    dialog_ex_set_right_button_text(dialog, delete_all ? "Delete All" : "Rebuild");
    dialog_ex_set_context(dialog, app);
    dialog_ex_set_result_callback(dialog, gamemenu_reset_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewDialogEx);
}

bool cfw_app_scene_interface_gamemenu_reset_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == DialogExResultRight) {
        bool delete_all =
            scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterfaceGamemenuReset);
        if(!(delete_all ? game_menu_save(app->storage, NULL, 0) : game_menu_reset(app->storage))) {
            cfw_app_gamemenu_save_error(app);
            return true;
        }
        cfw_app_empty_gamemenu_apps(app);
        if(delete_all) {
            app->gamemenu_source = GameMenuSourceCustom;
            app->gamemenu_apps_loaded = true;
        } else {
            cfw_app_load_gamemenu_apps(app);
        }
        app->gamemenu_app_index = 0;
        app->save_gamemenu_apps = false;
        cfw_settings.game_start_point = 0;
        app->save_settings = true;
    }
    if(event.event == DialogExResultLeft || event.event == DialogExResultRight) {
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void cfw_app_scene_interface_gamemenu_reset_on_exit(void* context) {
    CFWApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}
