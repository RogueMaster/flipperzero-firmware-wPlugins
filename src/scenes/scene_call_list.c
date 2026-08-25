#include "scene_call_list.h"

#include "../api_caller.h"

#define SCENE_CALL_LIST_EMPTY "Nessuna chiamata salvata"

static void api_caller_scene_call_list_item_callback(void* context, uint32_t index) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void api_caller_scene_call_list_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Chiamate salvate");

    if(app->call_history_count == 0) {
        submenu_add_item(
            app->submenu, SCENE_CALL_LIST_EMPTY, 0, api_caller_scene_call_list_item_callback, app);
    } else {
        for(uint8_t i = 0; i < app->call_history_count; i++) {
            FuriString* label = furi_string_alloc_printf(
                "%s %s", call_method_names[app->call_history[i].method], app->call_history[i].url);
            submenu_add_item(
                app->submenu,
                furi_string_get_cstr(label),
                i,
                api_caller_scene_call_list_item_callback,
                app);
            furi_string_free(label);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewWifiScan);
}

bool api_caller_scene_call_list_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    AppContext* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t index = event.event;
        if(index < app->call_history_count) {
            // Open the form in edit mode, pre-filled with the selected call
            app->call_form = app->call_history[index];
            app->call_edit_index = (uint8_t)index;
            scene_manager_set_scene_state(app->scene_manager, ApiCallerSceneCallAdd, 0);
            scene_manager_next_scene(app->scene_manager, ApiCallerSceneCallAdd);
            consumed = true;
        }
    }

    return consumed;
}

void api_caller_scene_call_list_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;
    submenu_reset(app->submenu);
}
