#include "flipcheckers.h"
#include "helpers/flipcheckers_haptic.h"
#include "helpers/flipcheckers_file.h"
#include "helpers/flipcheckers_sound.h"

bool flipcheckers_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipCheckers* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

void flipcheckers_tick_event_callback(void* context) {
    furi_assert(context);
    FlipCheckers* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

//leave app if back button pressed
bool flipcheckers_navigation_event_callback(void* context) {
    furi_assert(context);
    FlipCheckers* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void text_input_callback(void* context) {
    furi_assert(context);
    FlipCheckers* app = context;
    bool handled = false;

    // check that there is text in the input
    if(strlen(app->input_text) > 0) {
        if(app->input_state == FlipCheckersTextInputGame) {
            if(app->import_game == 1) {
                strncpy(app->import_game_text, app->input_text, TEXT_SIZE);

                uint8_t status = FlipCheckersStatusNone;
                if(status == FlipCheckersStatusNone) {
                    //notification_message(app->notification, &sequence_blink_cyan_100);
                    flipcheckers_play_happy_bump(app);
                } else {
                    //notification_message(app->notification, &sequence_blink_red_100);
                    flipcheckers_play_long_bump(app);
                }
            }
            // reset input state
            app->input_state = FlipCheckersTextInputDefault;
            handled = true;
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipCheckersViewIdMenu);
        }
    }

    if(!handled) {
        // reset input state
        app->input_state = FlipCheckersTextInputDefault;
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipCheckersViewIdMenu);
    }
}

FlipCheckers* flipcheckers_app_alloc() {
    FlipCheckers* app = malloc(sizeof(FlipCheckers));
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    //Turn backlight on, believe me this makes testing your app easier
    notification_message(app->notification, &sequence_display_backlight_on);

    //Scene additions
    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&flipcheckers_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flipcheckers_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, flipcheckers_tick_event_callback, 100);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flipcheckers_custom_event_callback);

    // Settings defaults
    app->haptic = FlipCheckersHapticOn;
    app->sound = FlipCheckersSoundOff;
    app->white_mode = FlipCheckersPlayerHuman;
    app->black_mode = FlipCheckersPlayerAI1;
    app->must_jump = FlipCheckersMustJumpOn;
    // Load saved settings (overrides defaults if file exists)
    flipcheckers_load_settings(app);

    // Main menu
    app->import_game = 0;

    // Text input
    app->input_state = FlipCheckersTextInputDefault;

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipCheckersViewIdMenu, submenu_get_view(app->submenu));
    app->flipcheckers_startscreen = flipcheckers_startscreen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipCheckersViewIdStartscreen,
        flipcheckers_startscreen_get_view(app->flipcheckers_startscreen));
    app->flipcheckers_scene_1 = flipcheckers_scene_1_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipCheckersViewIdScene1,
        flipcheckers_scene_1_get_view(app->flipcheckers_scene_1));
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipCheckersViewIdSettings,
        variable_item_list_get_view(app->variable_item_list));

    app->text_input = text_input_alloc();
    text_input_set_result_callback(
        app->text_input,
        text_input_callback,
        (void*)app,
        app->input_text,
        TEXT_BUFFER_SIZE,
        //clear default text
        true);
    text_input_set_header_text(app->text_input, "Input");
    view_dispatcher_add_view(
        app->view_dispatcher, FlipCheckersViewIdTextInput, text_input_get_view(app->text_input));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipCheckersViewIdAbout, text_box_get_view(app->text_box));

    //End Scene Additions

    return app;
}

void flipcheckers_app_free(FlipCheckers* app) {
    furi_assert(app);

    // Scene manager
    scene_manager_free(app->scene_manager);

    // View Dispatcher
    view_dispatcher_remove_view(app->view_dispatcher, FlipCheckersViewIdMenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCheckersViewIdStartscreen);
    flipcheckers_startscreen_free(app->flipcheckers_startscreen);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCheckersViewIdScene1);
    flipcheckers_scene_1_free(app->flipcheckers_scene_1);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCheckersViewIdSettings);
    variable_item_list_free(app->variable_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCheckersViewIdTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCheckersViewIdAbout);
    text_box_free(app->text_box);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    app->gui = NULL;
    app->notification = NULL;

    //Remove whatever is left
    //memzero(app, sizeof(FlipCheckers));
    free(app);
}

int32_t flipcheckers_app(void* p) {
    UNUSED(p);
    FlipCheckers* app = flipcheckers_app_alloc();

    // Disabled because causes exit on custom firmwares such as RM
    /*if(!furi_hal_region_is_provisioned()) {
        flipcheckers_app_free(app);
        return 1;
    }*/

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(
        app->scene_manager, FlipCheckersSceneStartscreen); //Start with start screen
    //scene_manager_next_scene(app->scene_manager, FlipCheckersSceneMenu); //if you want to directly start with Menu

    furi_hal_random_init();
    // furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(app->view_dispatcher);

    // furi_hal_power_suppress_charge_exit();
    flipcheckers_app_free(app);

    return 0;
}
