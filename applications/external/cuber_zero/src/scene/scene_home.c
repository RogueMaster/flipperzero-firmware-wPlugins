#include "src/cuberzero.h"

static void callbackItem(void* const context, const uint32_t index) {
    furi_check(context);
    scene_manager_handle_custom_event(((PCUBERZERO)context)->manager, index);
}

void SceneHomeEnter(void* const context) {
    furi_check(context);
    const PCUBERZERO instance = context;
    submenu_reset(instance->view.submenu);
    submenu_set_header(instance->view.submenu, "Cuber Zero");
    submenu_add_item(instance->view.submenu, "Timer", SCENE_TIMER, callbackItem, context);
    // ----- Test Only -----
    submenu_add_item(instance->view.submenu, "Session", SCENE_SESSION, callbackItem, context);
    // ---------------------
    submenu_set_selected_item(instance->view.submenu, instance->scene.home.index);
    view_dispatcher_switch_to_view(instance->dispatcher, VIEW_SUBMENU);
}

bool SceneHomeEvent(void* const context, const SceneManagerEvent event) {
    furi_check(context);

    if(event.type != SceneManagerEventTypeCustom || event.event >= COUNT_SCENE) {
        return 0;
    }

    ((PCUBERZERO)context)->scene.home.index =
        submenu_get_selected_item(((PCUBERZERO)context)->view.submenu);
    scene_manager_next_scene(((PCUBERZERO)context)->manager, event.event);
    return 1;
}
