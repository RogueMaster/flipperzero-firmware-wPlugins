#include "../pocketlab_i.h"

void pocketlab_scene_levelup_on_enter(void* context) {
    PocketLab* app = context;

    levelup_view_configure(
        app->levelup_view, "Level Up!", pocketlab_level_title(app->state.level), 75);

    pocketlab_sound_play(app->notifications, app->state.sound != 0, PocketLabSoundReward);
    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewLevelUp);
}

bool pocketlab_scene_levelup_on_event(void* context, SceneManagerEvent event) {
    PocketLab* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == PocketLabCustomEventLevelUpDone) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void pocketlab_scene_levelup_on_exit(void* context) {
    UNUSED(context);
}
