#include "../pocketlab_i.h"

typedef enum {
    MenuIndexStart,
    MenuIndexQuiz,
    MenuIndexProgress,
    MenuIndexSettings,
    MenuIndexAbout,
} MenuIndex;

void pocketlab_scene_menu_on_enter(void* context) {
    PocketLab* app = context;

    const char* items[] = {
        pocketlab_text(PocketLabTextMenuStart),
        pocketlab_text(PocketLabTextMenuQuiz),
        pocketlab_text(PocketLabTextMenuProgress),
        pocketlab_text(PocketLabTextMenuSettingsShort),
        pocketlab_text(PocketLabTextMenuAbout),
    };

    home_view_configure(
        app->home_view,
        pocketlab_text(PocketLabTextAppName),
        items,
        COUNT_OF(items),
        scene_manager_get_scene_state(app->scene_manager, PocketLabSceneMenu),
        app->state.xp,
        app->notifications,
        app->state.sound != 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewHome);
}

bool pocketlab_scene_menu_on_event(void* context, SceneManagerEvent event) {
    PocketLab* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, PocketLabSceneMenu, event.event);

    switch(event.event) {
    case MenuIndexStart:
        scene_manager_next_scene(app->scene_manager, PocketLabSceneLabs);
        break;
    case MenuIndexQuiz:
        scene_manager_next_scene(app->scene_manager, PocketLabSceneExam);
        break;
    case MenuIndexProgress:
        scene_manager_next_scene(app->scene_manager, PocketLabSceneProgress);
        break;
    case MenuIndexSettings:
        scene_manager_next_scene(app->scene_manager, PocketLabSceneSettings);
        break;
    case MenuIndexAbout:
        scene_manager_next_scene(app->scene_manager, PocketLabSceneAbout);
        break;
    default:
        break;
    }

    return true;
}

void pocketlab_scene_menu_on_exit(void* context) {
    PocketLab* app = context;
    scene_manager_set_scene_state(
        app->scene_manager, PocketLabSceneMenu, home_view_get_selected(app->home_view));
}
