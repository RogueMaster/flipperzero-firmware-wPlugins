#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <stdlib.h>

#include "views/sensor_view.h"

#define SENSOR_POLL_INTERVAL_MS 100
#define ADC_SAMPLES             10

// GPIO pin 16 (PC3) for analog sensor input
#define SENSOR_PIN_NUMBER 16

// Calibration defaults for Capacitive Moisture Sensor v1.2
// Higher ADC = drier (more resistance), Lower ADC = wetter (less resistance)
#define ADC_MAX_VALUE        4095
#define ADC_DRY_DEFAULT      3650
#define ADC_WET_DEFAULT      1700
#define ADC_STEP             50
#define SENSOR_MIN_THRESHOLD 300

#define CALIBRATION_FILE_PATH    APP_DATA_PATH("calibration.conf")
#define CALIBRATION_FILE_TYPE    "Moisture Sensor Calibration"
#define CALIBRATION_FILE_VERSION 1

// Scene IDs
typedef enum {
    MoistureSensorSceneMain,
    MoistureSensorSceneMenu,
    MoistureSensorScenePopup,
    MoistureSensorSceneCount,
} MoistureSensorScene;

// View IDs
typedef enum {
    MoistureSensorViewSensor,
    MoistureSensorViewMenu,
    MoistureSensorViewPopup,
} MoistureSensorView;

// Custom events
typedef enum {
    MoistureSensorEventMenuSelected,
    MoistureSensorEventSave,
    MoistureSensorEventReset,
    MoistureSensorEventPopupDone,
} MoistureSensorEvent;

typedef struct {
    // GUI
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    // Views
    SensorView* sensor_view;
    VariableItemList* variable_item_list;
    Popup* popup;

    // Hardware
    FuriHalAdcHandle* adc_handle;
    const GpioPin* gpio_pin;
    FuriHalAdcChannel adc_channel;

    // Sensor thread
    FuriThread* sensor_thread;
    FuriMutex* mutex;
    bool running;

    // Calibration values
    uint16_t cal_dry_value;
    uint16_t cal_wet_value;

    // Temporary edit values for menu
    uint16_t edit_dry_value;
    uint16_t edit_wet_value;

    // Menu items for updating display
    VariableItem* item_dry;
    VariableItem* item_wet;

    // Popup message storage
    const char* popup_message;
    bool popup_return_to_main;
} MoistureSensorApp;

// Scene handlers - declared here, implemented in moisture_sensor_scenes.c
void moisture_sensor_scene_main_on_enter(void* context);
bool moisture_sensor_scene_main_on_event(void* context, SceneManagerEvent event);
void moisture_sensor_scene_main_on_exit(void* context);

void moisture_sensor_scene_menu_on_enter(void* context);
bool moisture_sensor_scene_menu_on_event(void* context, SceneManagerEvent event);
void moisture_sensor_scene_menu_on_exit(void* context);

void moisture_sensor_scene_popup_on_enter(void* context);
bool moisture_sensor_scene_popup_on_event(void* context, SceneManagerEvent event);
void moisture_sensor_scene_popup_on_exit(void* context);

// Calibration functions
bool calibration_load(MoistureSensorApp* app);
bool calibration_save(MoistureSensorApp* app);

// Utility
uint8_t calculate_moisture_percent(uint16_t cal_dry, uint16_t cal_wet, uint16_t raw);
