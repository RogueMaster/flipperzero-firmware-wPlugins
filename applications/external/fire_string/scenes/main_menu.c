#include "../fire_string.h"

/** indicies for menu items*/
typedef enum {
    FireStringMenuSelection_Generate,
    FireStringMenuSelection_Settings,
    FireStringMenuSelection_LoadString,
    FireStringMenuSelection_About
} FireStringMenuSelection;

void fire_string_menu_callback_main_menu(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "fire_string_menu_callback_main_menu");
    FireString* app = context;
    switch(index) {
    case FireStringMenuSelection_Generate:
        // scene_manager_handle_custom_event(app->scene_manager, FireStringEvent_ShowTextView);
        scene_manager_handle_custom_event(
            app->scene_manager, FireStringEvent_ShowStringGeneratorView);
        break;
    case FireStringMenuSelection_Settings:
        scene_manager_handle_custom_event(
            app->scene_manager, FireStringEvent_ShowVariableItemList);
        break;
    case FireStringMenuSelection_LoadString:
        scene_manager_handle_custom_event(app->scene_manager, FireStringEvent_ShowFileBrowser);
        break;
    case FireStringMenuSelection_About:
        scene_manager_handle_custom_event(app->scene_manager, FireStringEvent_ShowAbout);
        break;
    }
}

/** resets the menu, gives it content, callbacks and selection enums */
void fire_string_scene_on_enter_main_menu(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_main_menu");
    FireString* app = context;

    menu_add_item(
        app->menu,
        "Generate",
        &A_Flame,
        FireStringMenuSelection_Generate,
        fire_string_menu_callback_main_menu,
        app);
    menu_add_item(
        app->menu,
        "Settings",
        &A_Settings_14,
        FireStringMenuSelection_Settings,
        fire_string_menu_callback_main_menu,
        app);
    menu_add_item(
        app->menu,
        "About",
        &A_About,
        FireStringMenuSelection_About,
        fire_string_menu_callback_main_menu,
        app);
    menu_add_item(
        app->menu,
        "Load Saved String",
        &A_Load,
        FireStringMenuSelection_LoadString,
        fire_string_menu_callback_main_menu,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Menu);
}

/** main menu event handler - switches scene based on the event */
bool fire_string_scene_on_event_main_menu(void* context, SceneManagerEvent event) {
    // FURI_LOG_T(TAG, "fire_string_scene_on_event_main_menu");
    FireString* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case FireStringEvent_ShowStringGeneratorView:
            if(app->settings->str_type == StrType_Words && app->dict->word_list == NULL) {
                scene_manager_next_scene(app->scene_manager, FireStringScene_Loading_Word_List);
            } else {
                scene_manager_next_scene(app->scene_manager, FireStringScene_Generate);
            }
            consumed = true;
            break;
        case FireStringEvent_ShowVariableItemList:
            scene_manager_next_scene(app->scene_manager, FireStringScene_Settings);
            consumed = true;
            break;
        case FireStringEvent_ShowFileBrowser:
            scene_manager_next_scene(app->scene_manager, FireStringScene_LoadString);
            break;
        case FireStringEvent_ShowAbout:
            scene_manager_next_scene(app->scene_manager, FireStringScene_About);
            break;
        default:
            break;
        }
        break;
    case SceneManagerEventTypeBack:
        consumed = true;
        view_dispatcher_stop(app->view_dispatcher);
        break;
    default:
        consumed = false;
        break;
    }
    return consumed;
}

void fire_string_scene_on_exit_main_menu(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_main_menu");
    furi_check(context);

    FireString* app = context;
    menu_reset(app->menu);
}
