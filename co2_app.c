/*
 * co2_app.c — entry point for CO2-monitor FAP.
 *
 * Architecture:
 *   ViewDispatcher with 3 views: Main, MainMenu, Settings.
 *   100 ms tick polls all sensors via unitemp_sensors_updateValues().
 *   Two hardcoded sensors: BME280 (I2C 0xEC) + MH-Z19C (PWM PA6).
 */
#include "co2_app_i.h"
#include <furi_hal_power.h>
#include <math.h>

/* ---- Global app pointer (extern declared in sensors/unitemp/unitemp.h) ---- */
App* app = NULL;

/* ---- Sensor value conversions (declared in unitemp.h) ---- */

void unitemp_celsiusToFahrenheit(Sensor* sensor) {
    sensor->temp       = sensor->temp * (9.0f / 5.0f) + 32.0f;
    sensor->heat_index = sensor->heat_index * (9.0f / 5.0f) + 32.0f;
}

static const float heat_index_consts[9] = {
    -42.379f,     2.04901523f,  10.14333127f, -0.22475541f, -0.00683783f,
    -0.05481717f, 0.00122874f,  0.00085282f,  -0.00000199f};

void unitemp_calculate_heat_index(Sensor* sensor) {
    float t = sensor->temp * (9.0f / 5.0f) + 32.0f;
    float h = sensor->hum;
    sensor->heat_index =
        (heat_index_consts[0] + heat_index_consts[1] * t + heat_index_consts[2] * h +
         heat_index_consts[3] * t * h + heat_index_consts[4] * t * t +
         heat_index_consts[5] * h * h + heat_index_consts[6] * t * t * h +
         heat_index_consts[7] * t * h * h + heat_index_consts[8] * t * t * h * h - 32.0f) *
        (5.0f / 9.0f);
}

static float calculateDewPoint(float temperature, float relativeHumidity) {
    float a = 17.27f, b = 237.7f;
    float tmp = (a * temperature) / (b + temperature) + logf(relativeHumidity / 100.0f);
    return (b * tmp) / (a - tmp);
}
void unitemp_rhToDewpointC(Sensor* sensor) {
    sensor->hum = calculateDewPoint(sensor->temp, sensor->hum);
}
void unitemp_rhToDewpointF(Sensor* sensor) {
    sensor->hum = calculateDewPoint(sensor->temp, sensor->hum) * (9.0f / 5.0f) + 32.0f;
}

void unitemp_pascalToMmHg(Sensor* sensor) { sensor->pressure *= 0.007500638f; }
void unitemp_pascalToKPa(Sensor* sensor)  { sensor->pressure /= 1000.0f; }
void unitemp_pascalToHPa(Sensor* sensor)  { sensor->pressure /= 100.0f; }
void unitemp_pascalToInHg(Sensor* sensor) { sensor->pressure *= 0.0002953007f; }

/* ---- Settings persistence ---- */

bool unitemp_saveSettings(void) {
    app->file_stream = file_stream_alloc(app->storage);
    FuriString* filepath = furi_string_alloc();
    furi_string_printf(filepath, "%s/%s", APP_PATH_FOLDER, APP_FILENAME_SETTINGS);
    storage_common_mkdir(app->storage, APP_PATH_FOLDER);
    if(!file_stream_open(
           app->file_stream,
           furi_string_get_cstr(filepath),
           FSAM_READ_WRITE,
           FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(APP_NAME, "Settings save error: %d", file_stream_get_error(app->file_stream));
        file_stream_close(app->file_stream);
        stream_free(app->file_stream);
        furi_string_free(filepath);
        return false;
    }
    stream_write_format(app->file_stream, "INFINITY_BACKLIGHT %d\n", app->settings.infinityBacklight);
    stream_write_format(app->file_stream, "TEMP_UNIT %d\n",     app->settings.temp_unit);
    stream_write_format(app->file_stream, "HUMIDITY_UNIT %d\n", app->settings.humidity_unit);
    stream_write_format(app->file_stream, "PRESSURE_UNIT %d\n", app->settings.pressure_unit);
    stream_write_format(app->file_stream, "HEAT_INDEX %d\n",    app->settings.heat_index);
    file_stream_close(app->file_stream);
    stream_free(app->file_stream);
    furi_string_free(filepath);
    FURI_LOG_I(APP_NAME, "Settings saved");
    return true;
}

bool unitemp_loadSettings(void) {
    app->file_stream = file_stream_alloc(app->storage);
    FuriString* filepath = furi_string_alloc();
    furi_string_printf(filepath, "%s/%s", APP_PATH_FOLDER, APP_FILENAME_SETTINGS);
    if(!file_stream_open(
           app->file_stream,
           furi_string_get_cstr(filepath),
           FSAM_READ_WRITE,
           FSOM_OPEN_EXISTING)) {
        if(file_stream_get_error(app->file_stream) == FSE_NOT_EXIST) {
            FURI_LOG_W(APP_NAME, "No settings file, saving defaults");
        }
        file_stream_close(app->file_stream);
        stream_free(app->file_stream);
        furi_string_free(filepath);
        unitemp_saveSettings();
        return false;
    }

    uint8_t file_size = (uint8_t)stream_size(app->file_stream);
    if(file_size == 0) {
        file_stream_close(app->file_stream);
        stream_free(app->file_stream);
        furi_string_free(filepath);
        unitemp_saveSettings();
        return false;
    }

    uint8_t* file_buf = malloc(file_size);
    memset(file_buf, 0, file_size);
    if(stream_read(app->file_stream, file_buf, file_size) != file_size) {
        free(file_buf);
        file_stream_close(app->file_stream);
        stream_free(app->file_stream);
        furi_string_free(filepath);
        return false;
    }

    FuriString* file = furi_string_alloc_set_str((char*)file_buf);
    size_t line_end = 0;
    while(line_end != (size_t)-1 && line_end != (size_t)(file_size - 1)) {
        char key[24] = {0};
        sscanf((char*)(file_buf + line_end), "%s", key);
        int p = 0;
        if(!strcmp(key, "INFINITY_BACKLIGHT")) {
            sscanf((char*)(file_buf + line_end), "INFINITY_BACKLIGHT %d", &p);
            app->settings.infinityBacklight = (bool)p;
        } else if(!strcmp(key, "TEMP_UNIT")) {
            sscanf((char*)(file_buf + line_end), "\nTEMP_UNIT %d", &p);
            app->settings.temp_unit = (tempMeasureUnit)p;
        } else if(!strcmp(key, "HUMIDITY_UNIT")) {
            sscanf((char*)(file_buf + line_end), "\nHUMIDITY_UNIT %d", &p);
            app->settings.humidity_unit = (humidityUnit)p;
        } else if(!strcmp(key, "PRESSURE_UNIT")) {
            sscanf((char*)(file_buf + line_end), "\nPRESSURE_UNIT %d", &p);
            app->settings.pressure_unit = (pressureMeasureUnit)p;
        } else if(!strcmp(key, "HEAT_INDEX")) {
            sscanf((char*)(file_buf + line_end), "\nHEAT_INDEX %d", &p);
            app->settings.heat_index = (bool)p;
        }
        line_end = furi_string_search_char(file, '\n', line_end + 1);
    }
    furi_string_free(file);
    free(file_buf);
    file_stream_close(app->file_stream);
    stream_free(app->file_stream);
    furi_string_free(filepath);
    FURI_LOG_I(APP_NAME, "Settings loaded");
    return true;
}

/* ---- Tick callback: poll all sensors ---- */

static void tick_callback(void* context) {
    UNUSED(context);
    if(app->sensors_ready) {
        unitemp_sensors_updateValues();
    }
}

/* ---- Application entry point ---- */

int32_t co2_app_main(void* p) {
    UNUSED(p);

    /* Allocate and zero-init app struct */
    app = malloc(sizeof(App));
    furi_check(app != NULL);
    memset(app, 0, sizeof(App));

    /* Open system records */
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage       = furi_record_open(RECORD_STORAGE);

    /* Default settings */
    app->settings.infinityBacklight = true;
    app->settings.temp_unit         = UT_TEMP_CELSIUS;
    app->settings.humidity_unit     = UT_HUMIDITY_RELATIVE;
    app->settings.pressure_unit     = UT_PRESSURE_HPA;
    app->settings.heat_index        = false;
    app->settings.lastOTGState      = furi_hal_power_is_otg_enabled();

    /* Load settings from SD (creates defaults if missing) */
    unitemp_loadSettings();

    /* Apply backlight setting */
    if(app->settings.infinityBacklight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    /* ViewDispatcher */
    app->view_dispatcher = view_dispatcher_alloc();

    /* Allocate views (need app->view_dispatcher already set) */
    view_main_alloc();
    view_main_menu_alloc();
    view_settings_alloc();

    /* Hardcode sensors: BME280 (I2C addr 0xEC = 0x76<<1) + MH-Z19C (PWM PA6) */
    app->sensors       = NULL;
    app->sensors_count = 0;

    Sensor* bme = unitemp_sensor_alloc("BME280", &BME280, "EC");
    if(bme) {
        bme->temp_offset = -20; /* -2.0°C: compensate OTG/MH-Z19C self-heating near BME280 */
        unitemp_sensors_add(bme);
    }

    Sensor* co2 = unitemp_sensor_alloc("MH-Z19C", &MHZ19C, "");
    if(co2) unitemp_sensors_add(co2);

    /* Initialize sensors (enables OTG power, sets up GPIO) */
    unitemp_sensors_init();

    /* Set up 100 ms polling tick */
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, tick_callback, furi_ms_to_ticks(100));

    /* Attach dispatcher to GUI and run */
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_main_switch();
    view_dispatcher_run(app->view_dispatcher);

    /* --- Cleanup --- */

    /* Restore backlight */
    if(app->settings.infinityBacklight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    }

    unitemp_sensors_deInit();
    unitemp_sensors_free();

    view_settings_free();
    view_main_menu_free();
    view_main_free();

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
    app = NULL;

    return 0;
}
