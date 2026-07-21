#include "ear_trainer_i.h"
#include "scenes/ear_scene.h"

static bool ear_custom_event_callback(void* context, uint32_t event) {
    EarTrainerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ear_navigation_event_callback(void* context) {
    EarTrainerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* Fires on the timer thread, so it only posts an event; all game-state
 * mutation stays on the dispatcher thread. */
static void feedback_timer_callback(void* context) {
    EarTrainerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, ETEventFeedbackDone);
}

static EarTrainerApp* ear_trainer_app_alloc(void) {
    EarTrainerApp* app = malloc(sizeof(EarTrainerApp));
    memset(app, 0, sizeof(EarTrainerApp));

    ear_progress_load(&app->progress);
    ear_settings_load(&app->settings);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->player = tone_player_alloc(app->notifications, &app->settings);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&ear_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, ear_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ear_navigation_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EarViewSubmenu, submenu_get_view(app->submenu));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EarViewWidget, widget_get_view(app->widget));
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EarViewVarItemList, variable_item_list_get_view(app->var_item_list));
    app->quiz_view = quiz_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EarViewQuiz, quiz_view_get_view(app->quiz_view));

    app->feedback_timer = furi_timer_alloc(feedback_timer_callback, FuriTimerTypeOnce, app);

    return app;
}

static void ear_trainer_app_free(EarTrainerApp* app) {
    /* Reverse order of allocation: timer, views (removed before their modules
     * are freed), dispatcher/manager, records, player. */
    furi_timer_stop(app->feedback_timer);
    furi_timer_free(app->feedback_timer);

    view_dispatcher_remove_view(app->view_dispatcher, EarViewQuiz);
    quiz_view_free(app->quiz_view);
    view_dispatcher_remove_view(app->view_dispatcher, EarViewVarItemList);
    variable_item_list_free(app->var_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, EarViewWidget);
    widget_free(app->widget);
    view_dispatcher_remove_view(app->view_dispatcher, EarViewSubmenu);
    submenu_free(app->submenu);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    tone_player_free(app->player);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t ear_trainer_app(void* p) {
    UNUSED(p);
    EarTrainerApp* app = ear_trainer_app_alloc();
    scene_manager_next_scene(app->scene_manager, EarSceneMenu);
    view_dispatcher_run(app->view_dispatcher);
    ear_trainer_app_free(app);
    return 0;
}
