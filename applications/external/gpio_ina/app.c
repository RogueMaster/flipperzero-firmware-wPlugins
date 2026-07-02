/*
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
  *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <furi.h>

#include "app.h"
#include "scenes/scenes.h"

#include "sensor/ina209_driver.h"
#include "sensor/ina219_driver.h"
#include "sensor/ina226_driver.h"
#include "sensor/ina228_driver.h"

#define TICK_EVENT_PERIOD    5 // Sensor read period in ms
#define SCREEN_UPDATE_PERIOD 200 // Screen update period in ms

static bool app_custom_event_callback(void* context, uint32_t event) {
    App* app = (App*)context;
    furi_assert(app != NULL);

    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool app_back_event_callback(void* context) {
    App* app = (App*)context;
    furi_assert(app != NULL);

    bool handled = scene_manager_handle_back_event(app->scene_manager);

    if(!handled) {
        scene_manager_next_scene(app->scene_manager, SceneDialogExit);
    }

    return true;
}

// Set RGB LED to white for 1ms
const NotificationSequence sequence_blink = {
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_delay_1,
    NULL,
};

static bool is_charging(App* app) {
    PowerInfo power_info;
    power_get_info(app->power, &power_info);
    return power_info.is_charging;
}

static void app_tick_event_callback(void* context) {
    App* app = (App*)context;
    furi_assert(app != NULL);

    static int tick_counter = 1;

    if(app->sensor != NULL) {
        app->sensor->tick(app->sensor);

        SensorState sensor_state;
        app->sensor->get_state(app->sensor, &sensor_state);

        bool new_measurement = sensor_state.ticks != app->last_measurement_ticks;
        app->last_measurement_ticks = sensor_state.ticks;

        if(new_measurement) {
            if(app->config.led_blinking && !is_charging(app)) {
                notification_message(app->notifications, &sequence_blink);
            }

            if(app->datalog != NULL) {
                datalog_append_record(app->datalog, &sensor_state);
                // datalog_screen_update(app->datalog_screen, app->datalog);
            }
        }
    }

    if(--tick_counter == 0) {
        tick_counter = SCREEN_UPDATE_PERIOD / TICK_EVENT_PERIOD;
        scene_manager_handle_tick_event(app->scene_manager);
        datalog_screen_update(app->datalog_screen, app->datalog);
    }
}

void app_restart_sensor_driver(App* app) {
    if(app->sensor != NULL) {
        app->sensor->free(app->sensor);
        app->sensor = NULL;
    }

    switch(app->config.sensor_type) {
    /*
    case SensorType_INA209:
        Ina226Config ina209_config = {
            .i2c_address = app->config.i2c_address,
            .shunt_resistor = app->config.shunt_resistor,
        };
        sensor->driver = ina209_driver_alloc(&ina209_config);
        break;
    */
    case SensorType_INA219:
        Ina219Config ina219_config = {
            .i2c_address = app->config.i2c_address,
            .shunt_resistor = app->config.shunt_resistor,
        };
        app->sensor = ina219_driver_alloc(&ina219_config);
        break;

    case SensorType_INA226:
        Ina226ConvTime ina226_conv_time[SensorPrecision_count] = {
            [SensorPrecision_Low] = Ina226ConvTime_140us,
            [SensorPrecision_Medium] = Ina226ConvTime_588us,
            [SensorPrecision_High] = Ina226ConvTime_2116us,
            [SensorPrecision_Max] = Ina226ConvTime_8244us,
        };
        Ina226Averaging ina226_averaging[SensorAveraging_count] = {
            [SensorAveraging_Medium] = Ina226Averaging_256,
            [SensorAveraging_Max] = Ina226Averaging_1024,
        };
        Ina226Config ina226_config = {
            .i2c_address = app->config.i2c_address,
            .shunt_resistor = app->config.shunt_resistor,
            .averaging = ina226_averaging[app->config.sensor_averaging],
            .vbus_conv_time = ina226_conv_time[app->config.voltage_precision],
            .vshunt_conv_time = ina226_conv_time[app->config.current_precision],
        };
        app->sensor = ina226_driver_alloc(&ina226_config);
        break;
    case SensorType_INA228:
        Ina228ConvTime ina228_conv_time[SensorPrecision_count] = {
            [SensorPrecision_Low] = Ina228ConvTime_50us,
            [SensorPrecision_Medium] = Ina228ConvTime_280us,
            [SensorPrecision_High] = Ina228ConvTime_1052us,
            [SensorPrecision_Max] = Ina228ConvTime_4120us,
        };
        Ina228Averaging ina228_averaging[SensorAveraging_count] = {
            [SensorAveraging_Medium] = Ina228Averaging_256,
            [SensorAveraging_Max] = Ina228Averaging_1024,
        };
        Ina228Config ina228_config = {
            .i2c_address = app->config.i2c_address,
            .shunt_resistor = app->config.shunt_resistor,
            .averaging = ina228_averaging[app->config.sensor_averaging],
            .vbus_conv_time = ina228_conv_time[app->config.voltage_precision],
            .vshunt_conv_time = ina228_conv_time[app->config.current_precision],
            .adc_range = Ina228AdcRange_160mV,
        };
        app->sensor = ina228_driver_alloc(&ina228_config);
        break;

    default:
        break;
    }
}

static App* app_alloc() {
    FURI_LOG_T(TAG, "Starting application...");

    // Allocate the application state
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    // Open the GUI
    app->gui = furi_record_open(RECORD_GUI);
    // Open storage
    app->storage = furi_record_open(RECORD_STORAGE);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->power = furi_record_open(RECORD_POWER);

    // Initialize the application configuration
    app_config_init(&app->config);
    app_config_load(&app->config, app->storage);

    // Initialize scene manager
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);

    // Initialize view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, app_tick_event_callback, TICK_EVENT_PERIOD);

    // Attach view dispatcher to the GUI
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Allocate view
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewVariableList,
        variable_item_list_get_view(app->var_item_list));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewNumberInput, number_input_get_view(app->number_input));

    app->current_gauge = current_gauge_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewCurrentGauge, current_gauge_get_view(app->current_gauge));

    app->datalog_screen = datalog_screen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewDatalog, datalog_screen_get_view(app->datalog_screen));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewWiring, popup_get_view(app->popup));

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewDialog, dialog_ex_get_view(app->dialog));

    app_restart_sensor_driver(app);

    FURI_LOG_T(TAG, "Application started");

    return app;
}

static void app_free(App* app) {
    FURI_LOG_T(TAG, "Stopping application...");

    datalog_close(app->datalog);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, AppViewVariableList);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewWiring);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewDialog);
    dialog_ex_free(app->dialog);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewDatalog);
    datalog_screen_free(app->datalog_screen);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewCurrentGauge);
    current_gauge_free(app->current_gauge);

    // Free sensor driver
    if(app->sensor != NULL) {
        app->sensor->free(app->sensor);
    }

    // Free view dispatcher and scene manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Save the application configuration
    app_config_save(&app->config, app->storage);

    furi_record_close(RECORD_POWER);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app);

    FURI_LOG_T(TAG, "Application stopped");
}

static void app_run(App* app) {
    // Switch to the gauge screen
    scene_manager_next_scene(app->scene_manager, SceneGauge);

    FURI_LOG_D(TAG, "Running view dispatcher...");
    view_dispatcher_run(app->view_dispatcher);
}

// Application entrypoint called by the Furi runtime
int32_t app_startup(void* p) {
    UNUSED(p);

    App* app = app_alloc();
    app_run(app);
    app_free(app);

    return 0;
}
