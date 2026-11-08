#include "loteria_zero.h"
#include "scenes/scenes.h"

#include <stdlib.h>

/* Declared in scene_game.c */
void loteria_game_view_setup(View* view, LoteriaApp* app);

void loteria_app_save(LoteriaApp* app) {
    loteria_persist_save(&app->persist);
}

void loteria_notify_card(LoteriaApp* app) {
    if(app->notifications) {
        notification_message(app->notifications, &sequence_single_vibro);
    }
}

static void loteria_auto_timer_callback(void* context) {
    LoteriaApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoteriaCustomEventGameAutoTick);
}

void loteria_auto_timer_apply(LoteriaApp* app) {
    if(!app->auto_timer) return;
    furi_timer_stop(app->auto_timer);
    uint32_t ms = loteria_auto_mode_ms(app->persist.auto_mode);
    if(ms > 0 && !app->game.paused && app->game.active && !app->game.finished) {
        furi_timer_start(app->auto_timer, furi_ms_to_ticks(ms));
    }
}

static bool loteria_custom_event_callback(void* context, uint32_t event) {
    LoteriaApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool loteria_navigation_event_callback(void* context) {
    LoteriaApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static LoteriaApp* loteria_app_alloc(void) {
    LoteriaApp* app = malloc(sizeof(LoteriaApp));
    memset(app, 0, sizeof(LoteriaApp));

    loteria_persist_load(&app->persist);
    if(app->persist.has_saved_game) {
        app->game = app->persist.saved_game;
    } else {
        loteria_game_init(&app->game);
    }

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&loteria_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, loteria_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, loteria_navigation_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoteriaViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoteriaViewWidget, widget_get_view(app->widget));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        LoteriaViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->game_view = view_alloc();
    loteria_game_view_setup(app->game_view, app);
    view_dispatcher_add_view(app->view_dispatcher, LoteriaViewGame, app->game_view);

    app->auto_timer = furi_timer_alloc(loteria_auto_timer_callback, FuriTimerTypePeriodic, app);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void loteria_app_free(LoteriaApp* app) {
    furi_timer_stop(app->auto_timer);
    furi_timer_free(app->auto_timer);

    view_dispatcher_remove_view(app->view_dispatcher, LoteriaViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, LoteriaViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, LoteriaViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, LoteriaViewGame);

    submenu_free(app->submenu);
    widget_free(app->widget);
    variable_item_list_free(app->variable_item_list);
    view_free(app->game_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t loteria_zero_app(void* p) {
    UNUSED(p);
    LoteriaApp* app = loteria_app_alloc();

    if(app->persist.skip_welcome) {
        scene_manager_next_scene(app->scene_manager, LoteriaSceneMainMenu);
    } else {
        scene_manager_next_scene(app->scene_manager, LoteriaSceneWelcome);
    }

    view_dispatcher_run(app->view_dispatcher);

    app->persist.saved_game = app->game;
    app->persist.has_saved_game = loteria_game_has_save(&app->game);
    loteria_app_save(app);

    loteria_app_free(app);
    return 0;
}
