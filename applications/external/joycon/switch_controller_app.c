#include "switch_controller_app.h"
#include <storage/storage.h>

// Scene handlers array
void (*const switch_controller_scene_on_enter_handlers[])(void*) = {
    switch_controller_scene_on_enter_menu,
    switch_controller_scene_on_enter_controller,
    switch_controller_scene_on_enter_recording,
    switch_controller_scene_on_enter_playing,
    switch_controller_scene_on_enter_macro_name,
    switch_controller_scene_on_enter_macro_list,
};

bool (*const switch_controller_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    switch_controller_scene_on_event_menu,
    switch_controller_scene_on_event_controller,
    switch_controller_scene_on_event_recording,
    switch_controller_scene_on_event_playing,
    switch_controller_scene_on_event_macro_name,
    switch_controller_scene_on_event_macro_list,
};

void (*const switch_controller_scene_on_exit_handlers[])(void*) = {
    switch_controller_scene_on_exit_menu,
    switch_controller_scene_on_exit_controller,
    switch_controller_scene_on_exit_recording,
    switch_controller_scene_on_exit_playing,
    switch_controller_scene_on_exit_macro_name,
    switch_controller_scene_on_exit_macro_list,
};

// Scene manager handlers
static const SceneManagerHandlers switch_controller_scene_manager_handlers = {
    .on_enter_handlers = switch_controller_scene_on_enter_handlers,
    .on_event_handlers = switch_controller_scene_on_event_handlers,
    .on_exit_handlers = switch_controller_scene_on_exit_handlers,
    .scene_num = SwitchControllerSceneNum,
};

// Custom events
typedef enum {
    SwitchControllerEventBack = 0,
} SwitchControllerEvent;

// View dispatcher callbacks
static bool switch_controller_view_dispatcher_navigation_event_callback(void* context) {
    furi_assert(context);
    SwitchControllerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool
    switch_controller_view_dispatcher_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SwitchControllerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// Allocate app
static SwitchControllerApp* switch_controller_app_alloc() {
    SwitchControllerApp* app = malloc(sizeof(SwitchControllerApp));

    // Create storage folder
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SWITCH_CONTROLLER_APP_PATH_FOLDER);
    furi_record_close(RECORD_STORAGE);

    // Records
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, switch_controller_view_dispatcher_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, switch_controller_view_dispatcher_custom_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Scene manager
    app->scene_manager = scene_manager_alloc(&switch_controller_scene_manager_handlers, app);

    // Views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SwitchControllerViewSubmenu, submenu_get_view(app->submenu));

    app->macro_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        SwitchControllerViewMacroList,
        variable_item_list_get_view(app->macro_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SwitchControllerViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SwitchControllerViewPopup, popup_get_view(app->popup));

    // Custom views will be created in their respective scene files
    app->controller_view = NULL;
    app->recording_view = NULL;
    app->playing_view = NULL;

    // Initialize controller state
    usb_hid_switch_reset_state(&app->controller_state);
    app->usb_connected = false;
    app->usb_mode_prev = NULL;

    // Initialize macro system
    app->current_macro = macro_alloc();
    app->recorder = macro_recorder_alloc();
    app->player = macro_player_alloc();
    memset(app->macro_name_buffer, 0, MACRO_NAME_MAX_LEN);

    // App state
    app->mode = AppModeController;
    app->temp_path = furi_string_alloc();
    app->timer = NULL;

    return app;
}

// Free app
static void switch_controller_app_free(SwitchControllerApp* app) {
    furi_assert(app);

    // Free macro system
    macro_player_free(app->player);
    macro_recorder_free(app->recorder);
    macro_free(app->current_macro);

    // Free timer if allocated
    if(app->timer) {
        furi_timer_free(app->timer);
    }

    // Free string
    furi_string_free(app->temp_path);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, SwitchControllerViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, SwitchControllerViewMacroList);
    variable_item_list_free(app->macro_list);

    view_dispatcher_remove_view(app->view_dispatcher, SwitchControllerViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, SwitchControllerViewPopup);
    popup_free(app->popup);

    if(app->controller_view) {
        view_dispatcher_remove_view(app->view_dispatcher, SwitchControllerViewController);
        view_free(app->controller_view);
    }

    if(app->recording_view) {
        view_dispatcher_remove_view(app->view_dispatcher, SwitchControllerViewRecording);
        view_free(app->recording_view);
    }

    if(app->playing_view) {
        view_dispatcher_remove_view(app->view_dispatcher, SwitchControllerViewPlaying);
        view_free(app->playing_view);
    }

    // Free scene manager and view dispatcher
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Close records
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

// Entry point
int32_t switch_controller_app(void* p) {
    UNUSED(p);

    SwitchControllerApp* app = switch_controller_app_alloc();

    // Start with menu scene
    scene_manager_next_scene(app->scene_manager, SwitchControllerSceneMenu);

    // Run view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    switch_controller_app_free(app);

    return 0;
}
