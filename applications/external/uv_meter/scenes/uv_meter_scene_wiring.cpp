#include "uv_meter_app_i.hpp"
#include "uv_meter_event.hpp"

static void uv_meter_scene_wiring_enter_settings_callback(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    view_dispatcher_send_custom_event(app->view_dispatcher, UVMeterCustomEventSceneEnterSettings);
}

void uv_meter_scene_wiring_on_enter(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);

    uv_meter_wiring_set_enter_settings_callback(
        app->uv_meter_wiring_view, uv_meter_scene_wiring_enter_settings_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, UVMeterViewWiring);
}

bool uv_meter_scene_wiring_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == UVMeterCustomEventSceneEnterSettings) {
            scene_manager_next_scene(app->scene_manager, UVMeterSceneSettings);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        uint32_t current_time = furi_get_tick();

        // Check once per second if AS7331 sensor is available
        if(current_time - app->app_state->last_sensor_check_timestamp >= furi_ms_to_ticks(1000)) {
            furi_mutex_acquire(app->app_state->as7331_mutex, FuriWaitForever);

            if(!app->app_state->as7331_initialized) {
                // Initialize the sensor, also wake it up if in power down mode
                uint8_t i2c_address = 0x0;
                switch(app->app_state->i2c_address) {
                case UVMeterI2CAddressAuto:
                    break;
                case UVMeterI2CAddress74:
                    i2c_address = DefaultI2CAddr;
                    break;
                case UVMeterI2CAddress75:
                    i2c_address = SecondaryI2CAddr;
                    break;
                case UVMeterI2CAddress76:
                    i2c_address = TertiaryI2CAddr;
                    break;
                case UVMeterI2CAddress77:
                    i2c_address = QuaternaryI2CAddr;
                    break;
                }
                app->app_state->as7331_initialized = app->app_state->as7331.init(i2c_address);
            }
            if(!app->app_state->as7331_initialized || !app->app_state->as7331.deviceReady()) {
                app->app_state->as7331_initialized = false;
            } else {
                // Set default Gain and Integration Time
                app->app_state->as7331.setGain(GAIN_8);
                app->app_state->as7331.setIntegrationTime(TIME_128MS);
            }
            furi_mutex_release(app->app_state->as7331_mutex);

            if(app->app_state->as7331_initialized) {
                scene_manager_next_scene(app->scene_manager, UVMeterSceneData);
                consumed = true;
            }

            app->app_state->last_sensor_check_timestamp = current_time;
        }

    } else if(event.type == SceneManagerEventTypeBack) {
        // Always quit app in wiring scene
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void uv_meter_scene_wiring_on_exit(void* context) {
    UNUSED(context);
}
