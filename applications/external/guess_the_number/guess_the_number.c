#include "guess_the_number.h"

bool game_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    GameApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

void game_tick_event_callback(void* context) {
    furi_assert(context);
    GameApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

bool game_navigation_event_callback(void* context) {
    furi_assert(context);
    GameApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

GameApp* game_app_alloc() {
    GameApp* app = malloc(sizeof(GameApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    notification_message(app->notification, &sequence_display_backlight_on);

    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&game_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, game_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, game_tick_event_callback, 100);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, game_custom_event_callback);
    app->submenu = submenu_alloc();

    app->haptic = 1;
    app->speaker = 1;
    app->led = 1;
    app->save_settings = 1;

    game_read_settings(app);

    view_dispatcher_add_view(app->view_dispatcher, GameViewIdMenu, submenu_get_view(app->submenu));
    app->game_startscreen = game_startscreen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GameViewIdStartscreen,
        game_startscreen_get_view(app->game_startscreen));
    app->game_instructions = game_instructions_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GameViewIdInstructions,
        game_instructions_get_view(app->game_instructions));
    app->game_play = game_play_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GameViewIdPlay, game_play_get_view(app->game_play));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GameViewIdSettings,
        variable_item_list_get_view(app->variable_item_list));

    return app;
}

void game_app_free(GameApp* app) {
    furi_assert(app);

    scene_manager_free(app->scene_manager);

    view_dispatcher_remove_view(app->view_dispatcher, GameViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, GameViewIdStartscreen);
    view_dispatcher_remove_view(app->view_dispatcher, GameViewIdInstructions);
    view_dispatcher_remove_view(app->view_dispatcher, GameViewIdPlay);
    view_dispatcher_remove_view(app->view_dispatcher, GameViewIdSettings);

    submenu_free(app->submenu);
    game_startscreen_free(app->game_startscreen);
    game_instructions_free(app->game_instructions);
    game_play_free(app->game_play);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t guess_the_number_app(void* p) {
    UNUSED(p);
    GameApp* app = game_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, GameSceneStartscreen);

    furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(app->view_dispatcher);

    game_save_settings(app);

    furi_hal_power_suppress_charge_exit();
    game_app_free(app);

    return 0;
}
