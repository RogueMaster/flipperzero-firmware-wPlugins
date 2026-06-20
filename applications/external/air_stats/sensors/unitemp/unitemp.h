/*
 * unitemp.h — master header for CO2-monitor app.
 * Replaces the original Unitemp plugin header.
 * Sensor drivers include this via "../unitemp.h".
 */
#ifndef UNITEMP
#define UNITEMP

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <toolbox/stream/file_stream.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

/* ---- Application constants ---- */
#define APP_NAME              "AirStats"
#define APP_PATH_FOLDER       "/ext/apps_data/air_stats"
#define APP_FILENAME_SETTINGS "settings.cfg"
#define APP_FILENAME_SENSORS  "sensors.cfg"
#define BUFF_SIZE             32

/* ---- Debug macro ---- */
#define UNITEMP_D
#ifdef FURI_DEBUG
#define UNITEMP_DEBUG(msg, ...) FURI_LOG_D(APP_NAME, msg, ##__VA_ARGS__)
#else
#define UNITEMP_DEBUG(msg, ...)
#endif

/* ---- Settings enums (from original unitemp.h) ---- */
typedef enum {
    UT_TEMP_CELSIUS,
    UT_TEMP_FAHRENHEIT,
    UT_TEMP_COUNT
} tempMeasureUnit;

typedef enum {
    UT_PRESSURE_MM_HG,
    UT_PRESSURE_IN_HG,
    UT_PRESSURE_KPA,
    UT_PRESSURE_HPA,
    UT_PRESSURE_COUNT
} pressureMeasureUnit;

typedef enum {
    UT_HUMIDITY_RELATIVE,
    UT_HUMIDITY_DEWPOINT,
    UT_HUMIDITY_COUNT
} humidityUnit;

typedef enum {
    CO2_TYPE_PWM = 0,
    CO2_TYPE_UART = 1
} Co2SensorType;

typedef struct {
    uint8_t backlight_mode; /* 0=Off 1=Auto 2=1m 3=5m 4=10m 5=20m 6=60m 7=Inf */
    tempMeasureUnit temp_unit;
    humidityUnit humidity_unit;
    pressureMeasureUnit pressure_unit;
    bool heat_index;
    bool lastOTGState;
    Co2SensorType co2_type;
    uint8_t climate_type_idx;
    bool led_notify;
    bool sound_notify;
    uint8_t sound_volume;
    uint16_t co2_alert_threshold;
    uint16_t co2_pwm_range; /* PWM range: 2000..10000, step 1000 */
    bool debug_mode;
    bool show_status; /* show clock + battery on main screen */
} AppSettings;

/* ---- Sensor types and interfaces (provides Sensor, SensorType, GPIO etc.) ---- */
/* Interface headers include "../unitemp.h" → guard fires safely.            */
#include "Sensors.h"

/* ---- Application struct ---- */
typedef struct App {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Widget* widget;
    Popup* popup;
    /* Storage */
    Storage* storage;
    Stream* file_stream;
    /* Misc buffer */
    char buff[BUFF_SIZE];
    /* Sensors */
    Sensor** sensors;
    uint8_t sensors_count;
    bool sensors_ready;
    bool sensors_update;
    /* Settings */
    AppSettings settings;
    /* CO2 alert runtime state (not persisted) */
    bool co2_was_above;
    uint32_t last_alert_tick;
    uint32_t backlight_deadline;
} App;

/* ---- Global app pointer ---- */
extern App* app;

/* ---- Sensor value conversion prototypes ---- */
void unitemp_celsiusToFahrenheit(Sensor* sensor);
void unitemp_calculate_heat_index(Sensor* sensor);
void unitemp_rhToDewpointC(Sensor* sensor);
void unitemp_rhToDewpointF(Sensor* sensor);
void unitemp_pascalToMmHg(Sensor* sensor);
void unitemp_pascalToKPa(Sensor* sensor);
void unitemp_pascalToHPa(Sensor* sensor);
void unitemp_pascalToInHg(Sensor* sensor);

/* ---- Settings persistence ---- */
bool unitemp_saveSettings(void);
bool unitemp_loadSettings(void);

#endif /* UNITEMP */
