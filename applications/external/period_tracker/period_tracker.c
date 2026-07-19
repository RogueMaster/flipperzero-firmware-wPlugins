#include "period_tracker.h"
#include "period_tracker_alert.h"
#include <furi_hal.h>

// Scene handlers array
void (*const period_tracker_scene_on_enter_handlers[])(void*) = {
    period_tracker_scene_start_on_enter,
    period_tracker_scene_pin_entry_on_enter,
    period_tracker_scene_main_menu_on_enter,
    period_tracker_scene_add_girl_on_enter,
    period_tracker_scene_select_girl_on_enter,
    period_tracker_scene_girl_menu_on_enter,
    period_tracker_scene_edit_profile_on_enter,
    period_tracker_scene_log_event_on_enter,
    period_tracker_scene_date_picker_on_enter,
    period_tracker_scene_event_history_on_enter,
    period_tracker_scene_cycle_stats_on_enter,
    period_tracker_scene_view_predictions_on_enter,
    period_tracker_scene_daily_digest_on_enter,
    period_tracker_scene_settings_on_enter,
    period_tracker_scene_alert_settings_on_enter,
    period_tracker_scene_pin_settings_on_enter,
    period_tracker_scene_pin_setup_on_enter,
};

bool (*const period_tracker_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    period_tracker_scene_start_on_event,
    period_tracker_scene_pin_entry_on_event,
    period_tracker_scene_main_menu_on_event,
    period_tracker_scene_add_girl_on_event,
    period_tracker_scene_select_girl_on_event,
    period_tracker_scene_girl_menu_on_event,
    period_tracker_scene_edit_profile_on_event,
    period_tracker_scene_log_event_on_event,
    period_tracker_scene_date_picker_on_event,
    period_tracker_scene_event_history_on_event,
    period_tracker_scene_cycle_stats_on_event,
    period_tracker_scene_view_predictions_on_event,
    period_tracker_scene_daily_digest_on_event,
    period_tracker_scene_settings_on_event,
    period_tracker_scene_alert_settings_on_event,
    period_tracker_scene_pin_settings_on_event,
    period_tracker_scene_pin_setup_on_event,
};

void (*const period_tracker_scene_on_exit_handlers[])(void*) = {
    period_tracker_scene_start_on_exit,
    period_tracker_scene_pin_entry_on_exit,
    period_tracker_scene_main_menu_on_exit,
    period_tracker_scene_add_girl_on_exit,
    period_tracker_scene_select_girl_on_exit,
    period_tracker_scene_girl_menu_on_exit,
    period_tracker_scene_edit_profile_on_exit,
    period_tracker_scene_log_event_on_exit,
    period_tracker_scene_date_picker_on_exit,
    period_tracker_scene_event_history_on_exit,
    period_tracker_scene_cycle_stats_on_exit,
    period_tracker_scene_view_predictions_on_exit,
    period_tracker_scene_daily_digest_on_exit,
    period_tracker_scene_settings_on_exit,
    period_tracker_scene_alert_settings_on_exit,
    period_tracker_scene_pin_settings_on_exit,
    period_tracker_scene_pin_setup_on_exit,
};

// Scene manager configuration
const SceneManagerHandlers period_tracker_scene_handlers = {
    .on_enter_handlers = period_tracker_scene_on_enter_handlers,
    .on_event_handlers = period_tracker_scene_on_event_handlers,
    .on_exit_handlers = period_tracker_scene_on_exit_handlers,
    .scene_num = PeriodTrackerSceneCount,
};

// Navigation callback
static bool period_tracker_navigation_callback(void* context) {
    furi_assert(context);
    PeriodTrackerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// Custom event callback
static bool period_tracker_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    PeriodTrackerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// Create app data directory if it doesn't exist (SDK APP_DATA_PATH → /ext/apps_data/<appid>/)
void period_tracker_create_data_dir(PeriodTrackerApp* app) {
    Storage* storage = app->storage;

    FS_Error sd_status = storage_sd_status(storage);
    FURI_LOG_I(TAG, "SD card status: %d (0=OK)", sd_status);

    if(sd_status != FSE_OK) {
        FURI_LOG_E(TAG, "SD card not ready! Status: %d", sd_status);
        return;
    }

    FS_Error err = storage_common_mkdir(storage, APP_DATA_PATH(""));
    FURI_LOG_I(TAG, "mkdir APP_DATA_PATH result: %d (2=already exists is OK)", err);

    if(storage_common_exists(storage, APP_DATA_PATH(""))) {
        FURI_LOG_I(TAG, "App data directory ready");
    } else {
        FURI_LOG_E(TAG, "Failed to create app data directory!");
    }
}

// Allocate app
static PeriodTrackerApp* period_tracker_app_alloc() {
    FURI_LOG_I(TAG, "App allocation started");

    PeriodTrackerApp* app = malloc(sizeof(PeriodTrackerApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app memory");
        return NULL;
    }

    // Open storage
    app->storage = furi_record_open(RECORD_STORAGE);

    // Open notifications
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Create data directory
    period_tracker_create_data_dir(app);

    // Initialize alert system
    period_tracker_alert_init(app->storage);

    // Open GUI
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, period_tracker_navigation_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, period_tracker_custom_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Scene Manager
    app->scene_manager = scene_manager_alloc(&period_tracker_scene_handlers, app);

    // Submenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PeriodTrackerViewSubmenu, submenu_get_view(app->submenu));

    // Text Input
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PeriodTrackerViewTextInput, text_input_get_view(app->text_input));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PeriodTrackerViewWidget, widget_get_view(app->widget));

    // Variable Item List
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        PeriodTrackerViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    // Number Input
    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        PeriodTrackerViewNumberInput,
        number_input_get_view(app->number_input));

    // Text Box
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PeriodTrackerViewTextBox, text_box_get_view(app->text_box));
    app->text_box_store = furi_string_alloc();

    // Initialize PIN error timer (will be configured in PIN entry scene)
    app->pin_error_timer = NULL;

    // Allocate shared prediction buffer (40 predictions = ~6KB)
    // Reused throughout app lifetime to avoid repeated malloc/free on hot paths
    app->prediction_buffer_size = 40;
    app->prediction_buffer = malloc(sizeof(Prediction) * app->prediction_buffer_size);
    if(!app->prediction_buffer) {
        FURI_LOG_E(TAG, "Failed to allocate prediction buffer");
        // Continue without buffer - functions will fall back to local malloc
        app->prediction_buffer_size = 0;
    } else {
        FURI_LOG_I(
            TAG,
            "Allocated shared prediction buffer: %u predictions (%zu bytes)",
            app->prediction_buffer_size,
            sizeof(Prediction) * app->prediction_buffer_size);
    }

    FURI_LOG_I(TAG, "App allocation completed successfully");
    return app;
}

// Free app
static void period_tracker_app_free(PeriodTrackerApp* app) {
    furi_assert(app);

    FURI_LOG_I(TAG, "App cleanup started");

    // Deinitialize alert system
    period_tracker_alert_deinit();

    // Free PIN error timer if allocated
    if(app->pin_error_timer) {
        furi_timer_free(app->pin_error_timer);
        FURI_LOG_I(TAG, "PIN error timer freed");
    }

    // Free shared prediction buffer
    if(app->prediction_buffer) {
        free(app->prediction_buffer);
        app->prediction_buffer = NULL;
        FURI_LOG_I(TAG, "Shared prediction buffer freed");
    }

    // Remove views
    view_dispatcher_remove_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, PeriodTrackerViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, PeriodTrackerViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, PeriodTrackerViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, PeriodTrackerViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, PeriodTrackerViewTextBox);

    // Free views
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    widget_free(app->widget);
    variable_item_list_free(app->variable_item_list);
    number_input_free(app->number_input);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);

    // Free scene manager
    scene_manager_free(app->scene_manager);

    // Free view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Close records and set to NULL to prevent use-after-free
    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    furi_record_close(RECORD_STORAGE);
    app->storage = NULL;

    // Free app
    free(app);

    FURI_LOG_I(TAG, "App cleanup completed");
}

// Application entry point
int32_t period_tracker_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "=== Period Tracker App Starting ===");

    PeriodTrackerApp* app = period_tracker_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app, exiting");
        return -1;
    }

    // Start with the start scene
    scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneStart);

    // Run view dispatcher
    FURI_LOG_I(TAG, "Starting view dispatcher loop");
    view_dispatcher_run(app->view_dispatcher);

    // Free app
    FURI_LOG_I(TAG, "View dispatcher stopped, cleaning up");
    period_tracker_app_free(app);

    FURI_LOG_I(TAG, "=== Period Tracker App Exited ===");
    return 0;
}
