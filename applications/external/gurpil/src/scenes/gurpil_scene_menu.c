#include "include/app/gurpil_app.h"

// Main menu items, in the order they appear in the Submenu — see gurpil_scene_menu_on_enter.
typedef enum {
    GurpilMenuIndexPlay,
    GurpilMenuIndexHowToPlay,
    GurpilMenuIndexCredits,
} GurpilMenuIndex;

static const char *const GURPIL_MENU_LABEL_PLAY = "Play";
static const char *const GURPIL_MENU_LABEL_HOW_TO_PLAY = "How to play";
static const char *const GURPIL_MENU_LABEL_CREDITS = "Credits";

static void gurpil_scene_menu_submenu_callback(void *context, uint32_t index) {
    GurpilApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void gurpil_scene_menu_on_enter(void *context) {
    GurpilApp *app = context;

    submenu_add_item(app->menu, GURPIL_MENU_LABEL_PLAY, GurpilMenuIndexPlay,
                     gurpil_scene_menu_submenu_callback, app);
    submenu_add_item(app->menu, GURPIL_MENU_LABEL_HOW_TO_PLAY, GurpilMenuIndexHowToPlay,
                     gurpil_scene_menu_submenu_callback, app);
    submenu_add_item(app->menu, GURPIL_MENU_LABEL_CREDITS, GurpilMenuIndexCredits,
                     gurpil_scene_menu_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, GurpilViewIdMenu);
}

bool gurpil_scene_menu_on_event(void *context, SceneManagerEvent event) {
    GurpilApp *app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch (event.event) {
            case GurpilMenuIndexPlay:
                scene_manager_next_scene(app->scene_manager, GurpilSceneGame);
                break;
            case GurpilMenuIndexHowToPlay:
                scene_manager_next_scene(app->scene_manager, GurpilSceneHowToPlay);
                break;
            case GurpilMenuIndexCredits:
                scene_manager_next_scene(app->scene_manager, GurpilSceneCredits);
                break;
            default:
                consumed = false;
                break;
        }
    }

    // An unconsumed Back event (the only other kind that can reach a scene's on_event) falls
    // through to SceneManager's own default: pop this scene, and since Menu is the base of the
    // stack, that reports "no previous scene" back up to the app's navigation callback, which
    // stops the ViewDispatcher — i.e. Back at the menu exits the app.
    return consumed;
}

void gurpil_scene_menu_on_exit(void *context) {
    GurpilApp *app = context;
    submenu_reset(app->menu);
}
