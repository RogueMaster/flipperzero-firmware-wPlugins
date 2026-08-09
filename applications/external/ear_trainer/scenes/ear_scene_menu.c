#include "../ear_trainer_i.h"
#include "ear_scene.h"

typedef enum {
    MenuAscending,
    MenuDescending,
    MenuMixed,
    MenuChords,
    MenuScales,
    MenuReference,
    MenuProgress,
    MenuSettings,
    MenuAbout,
} MenuIndex;

static void menu_callback(void* context, uint32_t index) {
    EarTrainerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ear_scene_menu_on_enter(void* context) {
    EarTrainerApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Ear Trainer");
    submenu_add_item(submenu, "Intervals - ascending", MenuAscending, menu_callback, app);
    submenu_add_item(submenu, "Intervals - descending", MenuDescending, menu_callback, app);
    submenu_add_item(submenu, "Intervals - mixed", MenuMixed, menu_callback, app);
    submenu_add_item(submenu, "Chords", MenuChords, menu_callback, app);
    submenu_add_item(submenu, "Scales", MenuScales, menu_callback, app);
    submenu_add_item(submenu, "Interval reference", MenuReference, menu_callback, app);
    submenu_add_item(submenu, "Progress", MenuProgress, menu_callback, app);
    submenu_add_item(submenu, "Settings", MenuSettings, menu_callback, app);
    submenu_add_item(submenu, "About", MenuAbout, menu_callback, app);

    /* coming back from a mode lands on the mode you were using */
    if(app->mode < MODE_COUNT) submenu_set_selected_item(submenu, app->mode);

    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewSubmenu);
}

bool ear_scene_menu_on_event(void* context, SceneManagerEvent event) {
    EarTrainerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case MenuAscending:
    case MenuDescending:
    case MenuMixed:
    case MenuChords:
    case MenuScales:
        app->mode = event.event;
        scene_manager_next_scene(app->scene_manager, EarSceneLevelSelect);
        return true;
    case MenuReference:
        scene_manager_next_scene(app->scene_manager, EarSceneReference);
        return true;
    case MenuProgress:
        scene_manager_next_scene(app->scene_manager, EarSceneProgress);
        return true;
    case MenuSettings:
        scene_manager_next_scene(app->scene_manager, EarSceneSettings);
        return true;
    case MenuAbout:
        scene_manager_next_scene(app->scene_manager, EarSceneAbout);
        return true;
    default:
        return false;
    }
}

void ear_scene_menu_on_exit(void* context) {
    EarTrainerApp* app = context;
    submenu_reset(app->submenu);
}
