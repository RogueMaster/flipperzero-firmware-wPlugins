#include "scene_main.h"

#include "../api_caller.h"

typedef enum {
    MainMenuItemWifi,
    MainMenuItemCallAdd,
    MainMenuItemCallList,
} MainMenuItem;

static void api_caller_scene_main_item_callback(void* context, uint32_t index) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void api_caller_scene_main_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    variable_item_list_reset(app->var_item_list);
    variable_item_list_add(app->var_item_list, "Connessione", 0, NULL, NULL);
    variable_item_list_add(app->var_item_list, "Aggiungi chiamata", 0, NULL, NULL);
    variable_item_list_add(app->var_item_list, "Lista chiamate", 0, NULL, NULL);

    variable_item_list_set_enter_callback(
        app->var_item_list, api_caller_scene_main_item_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewMainMenu);
}

bool api_caller_scene_main_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    AppContext* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MainMenuItemWifi:
            scene_manager_next_scene(app->scene_manager, ApiCallerSceneWifi);
            consumed = true;
            break;
        case MainMenuItemCallAdd:
            // Fresh add mode: the form scene resets its buffers on enter
            app->call_edit_index = CALL_EDIT_INDEX_NONE;
            scene_manager_set_scene_state(app->scene_manager, ApiCallerSceneCallAdd, 0);
            scene_manager_next_scene(app->scene_manager, ApiCallerSceneCallAdd);
            consumed = true;
            break;
        case MainMenuItemCallList:
            scene_manager_next_scene(app->scene_manager, ApiCallerSceneCallList);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void api_caller_scene_main_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;
    variable_item_list_reset(app->var_item_list);
}
