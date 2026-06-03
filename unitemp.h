/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef UNITEMP_H_
#define UNITEMP_H_

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <dialogs/dialogs.h>

#include "scenes/unitemp_scene.h"
#include "helpers/unitemp_utils.h"

#include "views/view_no_sensors.h"
#include "views/view_single_sensor.h"
#include "views/view_temp_overview.h"
#include "views/view_sensor_info.h"

#include <power/power_service/power.h>

#include <storage/storage.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <toolbox/stream/file_stream.h>

#include "sensors.h"

/* Declaring Macro Substitutions */
//Application name
#define APP_NAME        "Unitemp"
//Application version
#define UNITEMP_APP_VER "2.1"

//Settings file name
#define APP_SETTINGS_FILENAME "unitemp.settings"
#define APP_SETTINGS_VERSION  (1)

//Text buffer size
#define TEXT_STORE_SIZE 32

//Debug macro (only if app builded in debug mode)
#ifdef FURI_DEBUG
#define UNITEMP_DEBUG(msg, ...) FURI_LOG_D(APP_NAME, msg, ##__VA_ARGS__)
#else
#define UNITEMP_DEBUG(msg, ...)
#endif

typedef enum {
    UnitempViewSubmenu,
    UnitempViewPopup,
    UnitempViewWidget,
    UnitempViewVariableList,
    UnitempViewNoSensors,
    UnitempViewSingleSensor,
    UnitempViewTempOverview,
    UnitempViewSensorInfo,
    UnitempViewTextInput,

    UnitempViewsCount
} UnitempView;

//Temperature units
typedef enum {
    UT_TEMP_CELSIUS,
    UT_TEMP_FAHRENHEIT,

    UT_TEMP_COUNT
} TempMeasureUnit;

//Pressure units
typedef enum {
    UT_PRESSURE_MM_HG,
    UT_PRESSURE_IN_HG,
    UT_PRESSURE_KPA,
    UT_PRESSURE_HPA,

    UT_PRESSURE_COUNT
} PressureMeasureUnit;

// Humidity units
typedef enum {
    UT_HUMIDITY_RELATIVE, // Relative humidity
    UT_HUMIDITY_DEW_POINT, // Dew point

    UT_HUMIDITY_COUNT // Number of humidity modes
} HumidityMeausureUnit;

typedef enum {
    CustomEventSwitchToSingleSensorView,
    CustomEventSwitchToTempOverviewView,
    CustomEventSwitchToSensorInfoView,
    CustomEventTextEditResult,
    CustomEventBack,
    CustomEventOneWireScan,
    CustomEventGPIOChanged,
    CustomEventI2CScan,
} UnitempCustomEventEnum;

/* Declaration of structures */
//Plugin settings
typedef struct {
    //Endless backlight operation
    bool infinity_backlight;
    //Temperature unit
    TempMeasureUnit temperature_unit;
    // Humidity units
    HumidityMeausureUnit humidity_unit;
    //Pressure unit
    PressureMeasureUnit pressure_unit;
    // Do calculate and show heat index
    bool heat_index;
    // Automatic 5V power supply
    bool otg_auto_on;
    // Latest OTG status
    bool otg_latest_state;
    // Light indication of the environment state
    bool environment_state_led_indication;
    // Sound and vibro indication of the environment state
    bool environment_state_sound_and_vibro_indication;
} UnitempSettings;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Popup* popup;
    Widget* widget;
    Submenu* submenu;
    VariableItemList* var_item_list;
    TextInput* text_input;
    DialogsApp* dialogs;

    NoSensors* no_sensors;
    SingleSensor* single_sensor;
    TempOverview* temp_overview;
    SensorInfo* sensor_info;
    Gui* gui;

    Storage* storage;
    Stream* file_stream;
    NotificationApp* notifications;

    FuriThread* reader_thread;
    Power* power;

    UnitempSettings* settings;
    Sensor* editable_sensor;
    EnvironmentState environment_state;

    char* txt_buff;
} UnitempApp;

/* Flags which the reader thread responds to */
typedef enum {
    UnitempThreadFlagExit = 1,
} UnitempThreadFlag;

void unitemp_submenu_callback(void* context, uint32_t index);
void unitemp_widget_callback(GuiButtonType result, InputType type, void* context);

bool unitemp_settings_load(void* context);
bool unitemp_settings_save(void* context);

#endif //UNITEMP_H_
