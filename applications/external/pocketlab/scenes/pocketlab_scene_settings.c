#include "../pocketlab_i.h"

void pocketlab_scene_settings_on_enter(void* context) {
    PocketLab* app = context;
    settings_view_configure(app->settings_view, &app->state, app->notifications);
    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewSettings);
}

bool pocketlab_scene_settings_on_event(void* context, SceneManagerEvent event) {
    PocketLab* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == PocketLabCustomEventSettingsReset) {
        scene_manager_next_scene(app->scene_manager, PocketLabSceneResetConfirm);
        return true;
    }
    return false;
}

void pocketlab_scene_settings_on_exit(void* context) {
    PocketLab* app = context;
    pocketlab_storage_save(&app->state);
}
