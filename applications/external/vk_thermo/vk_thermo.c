#include "vk_thermo.h"
#include "helpers/vk_thermo_storage.h"
#include "helpers/vk_thermo_nfc.h"
#include "views/vk_thermo_log_view.h"
#include "views/vk_thermo_graph_view.h"

static bool vk_thermo_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    VkThermo* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static void vk_thermo_tick_event_callback(void* context) {
    furi_assert(context);
    VkThermo* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static bool vk_thermo_navigation_event_callback(void* context) {
    furi_assert(context);
    VkThermo* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static VkThermo* vk_thermo_app_alloc(void) {
    VkThermo* app = malloc(sizeof(VkThermo));

    // Open records
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    // Keep backlight on while app is running
    notification_message(app->notification, &sequence_display_backlight_enforce_on);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, vk_thermo_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, vk_thermo_tick_event_callback, 250); // 250ms tick for animation
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, vk_thermo_custom_event_callback);

    // Scene Manager
    app->scene_manager = scene_manager_alloc(&vk_thermo_scene_handlers, app);

    // Set defaults
    app->haptic = VkThermoHapticOn;
    app->speaker = VkThermoSpeakerOn;
    app->led = VkThermoLedOn;
    app->temp_unit = VkThermoTempUnitCelsius;
    app->eh_timeout = VkThermoEhTimeout5s;
    app->debug = VkThermoDebugOff;

    // Initialize log
    vk_thermo_log_init(&app->log);

    // Load settings and log
    vk_thermo_read_settings(app);
    vk_thermo_log_load_csv(&app->log);

    // Allocate views
    app->scan_view = vk_thermo_scan_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VkThermoViewIdScan, vk_thermo_scan_view_get_view(app->scan_view));

    // Log view
    app->log_view = vk_thermo_log_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VkThermoViewIdLog, vk_thermo_log_view_get_view(app->log_view));

    // Graph view
    app->graph_view = vk_thermo_graph_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VkThermoViewIdGraph, vk_thermo_graph_view_get_view(app->graph_view));

    // Settings (Variable Item List)
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        VkThermoViewIdSettings,
        variable_item_list_get_view(app->variable_item_list));

    // Dialog for confirmations
    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VkThermoViewIdDialog, dialog_ex_get_view(app->dialog));

    // Allocate NFC
    app->nfc = vk_thermo_nfc_alloc();

    return app;
}

static void vk_thermo_app_free(VkThermo* app) {
    furi_assert(app);

    // Save settings
    vk_thermo_save_settings(app);

    // Free NFC
    vk_thermo_nfc_free(app->nfc);

    // Scene manager
    scene_manager_free(app->scene_manager);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, VkThermoViewIdScan);
    vk_thermo_scan_view_free(app->scan_view);

    view_dispatcher_remove_view(app->view_dispatcher, VkThermoViewIdLog);
    vk_thermo_log_view_free(app->log_view);

    view_dispatcher_remove_view(app->view_dispatcher, VkThermoViewIdGraph);
    vk_thermo_graph_view_free(app->graph_view);

    view_dispatcher_remove_view(app->view_dispatcher, VkThermoViewIdSettings);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, VkThermoViewIdDialog);
    dialog_ex_free(app->dialog);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Restore system backlight behavior
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    app->gui = NULL;
    app->notification = NULL;

    free(app);
}

int32_t vk_thermo_app(void* p) {
    UNUSED(p);

    VkThermo* app = vk_thermo_app_alloc();

    // Attach to GUI
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Start with scan scene
    scene_manager_next_scene(app->scene_manager, VkThermoSceneScan);

    // Suppress charge during NFC operations
    furi_hal_power_suppress_charge_enter();

    // Run view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    furi_hal_power_suppress_charge_exit();

    vk_thermo_app_free(app);

    return 0;
}
