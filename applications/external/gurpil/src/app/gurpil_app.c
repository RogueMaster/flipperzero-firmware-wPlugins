#include "include/app/gurpil_app.h"

#include "include/persistence/best_store.h"

#include <furi.h>
#include <stdlib.h>

/*
 * App-level wiring only: allocates the Gui/SceneManager/ViewDispatcher and the four views
 * (Submenu, the game's View, and two Widgets), registers them, forwards ViewDispatcher's custom
 * and navigation events into the SceneManager, and hands control to view_dispatcher_run until
 * Back at the menu stops it (see include/scenes/gurpil_scene.h for why that Backs all the way
 * out). All per-scene behavior — including the game's own FuriTimer — lives in src/scenes/.
 */

static bool gurpil_app_custom_event_callback(void *context, uint32_t event) {
    GurpilApp *app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool gurpil_app_navigation_event_callback(void *context) {
    GurpilApp *app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static GurpilApp *gurpil_app_alloc(void) {
    GurpilApp *app = malloc(sizeof(GurpilApp));
    furi_check(app != NULL);

    app->gui = furi_record_open(RECORD_GUI);
    app->best = best_store_load();
    app->game_timer = NULL;

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&gurpil_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher,
                                              gurpil_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher,
                                                  gurpil_app_navigation_event_callback);

    app->menu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GurpilViewIdMenu, submenu_get_view(app->menu));

    app->game_view = gurpil_game_view_alloc(app->view_dispatcher);
    view_dispatcher_add_view(app->view_dispatcher, GurpilViewIdGame,
                             gurpil_game_view_get_view(app->game_view));

    app->how_to_play = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GurpilViewIdHowToPlay,
                             widget_get_view(app->how_to_play));

    app->credits = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GurpilViewIdCredits,
                             widget_get_view(app->credits));

    return app;
}

static void gurpil_app_free(GurpilApp *app) {
    view_dispatcher_remove_view(app->view_dispatcher, GurpilViewIdMenu);
    submenu_free(app->menu);

    view_dispatcher_remove_view(app->view_dispatcher, GurpilViewIdGame);
    gurpil_game_view_free(app->game_view);

    view_dispatcher_remove_view(app->view_dispatcher, GurpilViewIdHowToPlay);
    widget_free(app->how_to_play);

    view_dispatcher_remove_view(app->view_dispatcher, GurpilViewIdCredits);
    widget_free(app->credits);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t gurpil_app_run(void) {
    GurpilApp *app = gurpil_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, GurpilSceneMenu);

    view_dispatcher_run(app->view_dispatcher);

    gurpil_app_free(app);
    return 0;
}
