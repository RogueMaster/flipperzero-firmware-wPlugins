#include "../seos_i.h"
#include "../seos_credential.h"

void seos_scene_file_select_on_enter(void* context) {
    Seos* seos = context;
    SeosCredential* seos_credential = seos->credential;
    // Process file_select return
    seos_credential_set_loading_callback(seos_credential, seos_show_loading_popup, seos);
    if(seos_credential_file_select(seos_credential)) {
        seos->flow_mode = FLOW_CRED;
        scene_manager_next_scene(seos->scene_manager, SeosSceneSavedMenu);
    } else {
        scene_manager_search_and_switch_to_previous_scene(seos->scene_manager, SeosSceneMainMenu);
    }
    seos_credential_set_loading_callback(seos_credential, NULL, seos);
}

bool seos_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void seos_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
