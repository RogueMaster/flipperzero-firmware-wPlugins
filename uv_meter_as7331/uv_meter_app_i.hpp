#pragma once

#include "uv_meter_app.hpp"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>

#include <furi.h>
#include <furi_hal.h>

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include "uv_meter_as7331_icons.h"
#include "scenes/uv_meter_scene.hpp"
#include "views/uv_meter_wiring.hpp"
#include "views/uv_meter_data.hpp"
#include "AS7331.hpp"

// Prevent compiler optimization for debugging (remove in production).
// #pragma GCC optimize("O0")

typedef struct {
    AS7331::Results results; /**< Processed measurement results */
    AS7331::RawResults raw_results; /**< Raw measurement results */
    AS7331 as7331; /**< AS7331 sensor object */
    bool as7331_initialized; /**< Flag indicating if sensor is initialized */
    FuriMutex* as7331_mutex; /**< Mutex for thread-safe data access */
    uint32_t last_sensor_check_timestamp; /**< Last time sensor availability was checked */
    // Settings
    UVMeterI2CAddress i2c_address;
    UVMeterUnit unit;
} UVMeterAppState;

struct UVMeterApp {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    // Views
    UVMeterWiring* uv_meter_wiring_view;
    VariableItemList* variable_item_list;
    UVMeterData* uv_meter_data_view;
    Widget* widget;

    UVMeterAppState* app_state; /**< Shared app_state */
};

typedef enum {
    UVMeterViewWiring,
    UVMeterViewVariableItemList,
    UVMeterViewData,
    UVMeterViewWidget,
} UVMeterView;
