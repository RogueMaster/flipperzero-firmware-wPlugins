#include "../portal_of_flipper_i.h"
#include "../pof_token.h"

#define TAG "PoFSceneFileSelect"

void pof_scene_file_select_on_enter(void* context) {
    PoFApp* pof = context;
    VirtualPortal* virtual_portal = pof->virtual_portal;

    PoFToken* pof_token = pof_token_alloc();

    // Process file_select return
    pof_token_set_loading_callback(pof_token, pof_show_loading_popup, pof);

    if(pof_token && pof_file_select(pof_token)) {
        virtual_portal_load_token(virtual_portal, pof_token);
        scene_manager_next_scene(pof->scene_manager, PoFSceneMain);
    } else {
        scene_manager_search_and_switch_to_previous_scene(pof->scene_manager, PoFSceneMain);
    }
    pof_token_free(pof_token);
    pof_token_set_loading_callback(pof_token, NULL, pof);
}

bool pof_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pof_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
