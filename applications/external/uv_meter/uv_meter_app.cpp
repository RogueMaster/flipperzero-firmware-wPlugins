
#include "uv_meter_app_i.hpp"

// Forward events from View Dispatcher to Scene Manager
static bool uv_meter_app_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool uv_meter_app_back_event_callback(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    bool value = scene_manager_handle_back_event(app->scene_manager);
    return value;
}

static void uv_meter_app_tick_callback(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    scene_manager_handle_tick_event(app->scene_manager);
}

static UVMeterApp* uv_meter_app_alloc() {
    UVMeterApp* app = new UVMeterApp();

    app->app_state = new UVMeterAppState();
    app->app_state->as7331_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->app_state->results.uv_a = 0;
    app->app_state->results.uv_b = 0;
    app->app_state->results.uv_c = 0;
    app->app_state->raw_results.uv_a = 0;
    app->app_state->raw_results.uv_b = 0;
    app->app_state->raw_results.uv_c = 0;
    app->app_state->last_sensor_check_timestamp = 0;
    app->app_state->as7331_initialized = false;
    app->app_state->i2c_address = UVMeterI2CAddressAuto;
    app->app_state->unit = UVMeterUnituW_cm_2;

    app->scene_manager = scene_manager_alloc(&uv_meter_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, uv_meter_app_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, uv_meter_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, uv_meter_app_tick_callback, furi_ms_to_ticks(150));

    app->uv_meter_wiring_view = uv_meter_wiring_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        UVMeterViewWiring,
        uv_meter_wiring_get_view(app->uv_meter_wiring_view));
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        UVMeterViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));
    app->uv_meter_data_view = uv_meter_data_alloc();
    uv_meter_data_set_sensor(
        app->uv_meter_data_view, &app->app_state->as7331, app->app_state->as7331_mutex);
    view_dispatcher_add_view(
        app->view_dispatcher, UVMeterViewData, uv_meter_data_get_view(app->uv_meter_data_view));
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UVMeterViewWidget, widget_get_view(app->widget));

    app->gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, UVMeterViewWiring);

    return app;
}

static void uv_meter_app_free(UVMeterApp* app) {
    furi_assert(app);
    furi_record_close(RECORD_GUI);
    view_dispatcher_remove_view(app->view_dispatcher, UVMeterViewWidget);
    widget_free(app->widget);
    view_dispatcher_remove_view(app->view_dispatcher, UVMeterViewData);
    uv_meter_data_free(app->uv_meter_data_view);
    view_dispatcher_remove_view(app->view_dispatcher, UVMeterViewVariableItemList);
    variable_item_list_free(app->variable_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, UVMeterViewWiring);
    uv_meter_wiring_free(app->uv_meter_wiring_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_mutex_free(app->app_state->as7331_mutex);
    delete app->app_state;
    delete app;
}

/**
 * @brief Main application entry point
 *
 * @param p Unused parameter
 * @return Exit code
 */
extern "C" int32_t uv_meter_app(void* p) {
    UNUSED(p);
    UVMeterApp* app = uv_meter_app_alloc();

    view_dispatcher_run(app->view_dispatcher);

    // Power down the sensor before exiting
    if(app->app_state->as7331_initialized) {
        app->app_state->as7331.setPowerDown(true);
        app->app_state->as7331_initialized = false;
    }

    uv_meter_app_free(app);
    return 0;
}
