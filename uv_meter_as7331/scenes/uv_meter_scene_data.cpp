#include "uv_meter_app_i.hpp"
#include "uv_meter_event.hpp"

#include <gui/view_dispatcher.h>

/**
 * @brief Start a new measurement
 *
 * Initializes a new measurement and updates the measurement state.
 *
 * @param app_state Pointer to the shared app_state
 */
static void start_measurement(UVMeterAppState* app_state) {
    // Start measurement
    app_state->as7331.setMeasurementMode(MEASUREMENT_MODE_COMMAND);
    app_state->as7331.startMeasurement();
}

/**
 * @brief Process the measurement results
 *
 * Retrieves the measurement results and updates the application state.
 *
 * @param app Pointer to the app
 */
static void process_measurement_results(UVMeterApp* app) {
    if(app->app_state->as7331.getResults(app->app_state->results, app->app_state->raw_results)) {
        FURI_LOG_D(
            "UV_Meter Data",
            "Irradiance UVA: %.2f µW/cm² UVB: %.2f µW/cm² UVC: %.2f µW/cm²",
            app->app_state->results.uv_a,
            app->app_state->results.uv_b,
            app->app_state->results.uv_c);
        uv_meter_data_set_results(
            app->uv_meter_data_view, &app->app_state->results, &app->app_state->raw_results);
    } else {
        FURI_LOG_E("UV_Meter Data", "Failed to get measurement results");
    }
}

static void uv_meter_scene_data_enter_settings_callback(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    view_dispatcher_send_custom_event(app->view_dispatcher, UVMeterCustomEventSceneEnterSettings);
}

void uv_meter_scene_data_on_enter(void* context) {
    auto* app = static_cast<UVMeterApp*>(context);

    uv_meter_update_from_sensor(app->uv_meter_data_view);
    uv_meter_data_set_unit(app->uv_meter_data_view, app->app_state->unit);

    uv_meter_data_set_enter_settings_callback(
        app->uv_meter_data_view, uv_meter_scene_data_enter_settings_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, UVMeterViewData);
}

bool uv_meter_scene_data_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == UVMeterCustomEventSceneEnterSettings) {
            scene_manager_next_scene(app->scene_manager, UVMeterSceneSettings);
            consumed = true;
        }

    } else if(event.type == SceneManagerEventTypeTick) {
        furi_mutex_acquire(app->app_state->as7331_mutex, FuriWaitForever);

        // Check if sensor got disconnected
        if(!app->app_state->as7331_initialized || !app->app_state->as7331.deviceReady()) {
            app->app_state->as7331_initialized = false;
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, UVMeterSceneWiring);
        } else {
            // Getting the status will put sensor into measurement state
            as7331_osr_status_reg_t status;
            app->app_state->as7331.getStatus(status);
            // Check if measurement is complete
            if(status.new_data) {
                process_measurement_results(app);
                start_measurement(app->app_state);
            }
            // This happens when measurement was interrupted by changing settings
            else if(status.osr.start_state != 1) {
                start_measurement(app->app_state);
            }
            // Handle overflows
            if(status.adc_overflow || status.result_overflow || status.out_conv_overflow) {
                FURI_LOG_E(
                    "UV Meter Data",
                    "Overflow detected! ADCOF (%d) MRESOF (%d) OUTCONVOF (%d)",
                    status.adc_overflow,
                    status.result_overflow,
                    status.out_conv_overflow);
            }
        }
        consumed = true;
        furi_mutex_release(app->app_state->as7331_mutex);

    } else if(event.type == SceneManagerEventTypeBack) {
        // Always quit app in data scene
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void uv_meter_scene_data_on_exit(void* context) {
    UNUSED(context);
}
