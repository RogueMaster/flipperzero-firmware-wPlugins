/*
 * air_stats.c — entry point for Air Stats FAP.
 *
 * Architecture:
 *   ViewDispatcher with 3 views: Main, MainMenu, Settings.
 *   100 ms tick polls all sensors via unitemp_sensors_updateValues().
 *   Two hardcoded sensors: BME280 (I2C 0xEC) + MH-Z19C (PWM PA6).
 */
#include "air_stats_i.h"
#include <furi_hal_light.h>
#include <furi_hal_power.h>
#include <math.h>

/* ---- Global app pointer (extern declared in sensors/unitemp/unitemp.h) ---- */
App* app = NULL;

/* ---- Sensor value conversions (declared in unitemp.h) ---- */

void unitemp_celsiusToFahrenheit(Sensor* sensor) {
    sensor->temp = sensor->temp * (9.0f / 5.0f) + 32.0f;
    sensor->heat_index = sensor->heat_index * (9.0f / 5.0f) + 32.0f;
}

static const float heat_index_consts[9] = {
    -42.379f,
    2.04901523f,
    10.14333127f,
    -0.22475541f,
    -0.00683783f,
    -0.05481717f,
    0.00122874f,
    0.00085282f,
    -0.00000199f};

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

void unitemp_pascalToMmHg(Sensor* sensor) {
    sensor->pressure *= 0.007500638f;
}
void unitemp_pascalToKPa(Sensor* sensor) {
    sensor->pressure /= 1000.0f;
}
void unitemp_pascalToHPa(Sensor* sensor) {
    sensor->pressure /= 100.0f;
}
void unitemp_pascalToInHg(Sensor* sensor) {
    sensor->pressure *= 0.0002953007f;
}

/* ---- Settings persistence ---- */

bool unitemp_saveSettings(void) {
    app->file_stream = file_stream_alloc(app->storage);
    FuriString* filepath = furi_string_alloc();
    furi_string_printf(filepath, "%s/%s", APP_PATH_FOLDER, APP_FILENAME_SETTINGS);
    storage_common_mkdir(app->storage, APP_PATH_FOLDER);
    if(!file_stream_open(
           app->file_stream, furi_string_get_cstr(filepath), FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(APP_NAME, "Settings save error: %d", file_stream_get_error(app->file_stream));
        file_stream_close(app->file_stream);
        stream_free(app->file_stream);
        furi_string_free(filepath);
        return false;
    }
    stream_write_format(app->file_stream, "BACKLIGHT_MODE %d\n", app->settings.backlight_mode);
    stream_write_format(app->file_stream, "TEMP_UNIT %d\n", app->settings.temp_unit);
    stream_write_format(app->file_stream, "HUMIDITY_UNIT %d\n", app->settings.humidity_unit);
    stream_write_format(app->file_stream, "PRESSURE_UNIT %d\n", app->settings.pressure_unit);
    stream_write_format(app->file_stream, "HEAT_INDEX %d\n", app->settings.heat_index);
    stream_write_format(app->file_stream, "CO2_TYPE %d\n", (int)app->settings.co2_type);
    stream_write_format(
        app->file_stream, "CLIMATE_TYPE_IDX %d\n", (int)app->settings.climate_type_idx);
    stream_write_format(app->file_stream, "LED_NOTIFY %d\n", app->settings.led_notify);
    stream_write_format(app->file_stream, "SOUND_NOTIFY %d\n", app->settings.sound_notify);
    stream_write_format(app->file_stream, "SOUND_VOLUME %d\n", app->settings.sound_volume);
    stream_write_format(
        app->file_stream, "CO2_ALERT_THRESHOLD %d\n", app->settings.co2_alert_threshold);
    stream_write_format(app->file_stream, "CO2_PWM_RANGE %d\n", app->settings.co2_pwm_range);
    stream_write_format(app->file_stream, "DEBUG_MODE %d\n", app->settings.debug_mode);
    stream_write_format(app->file_stream, "SHOW_STATUS %d\n", app->settings.show_status);
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
           app->file_stream, furi_string_get_cstr(filepath), FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
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
        if(!strcmp(key, "BACKLIGHT_MODE")) {
            sscanf((char*)(file_buf + line_end), "BACKLIGHT_MODE %d", &p);
            if(p >= 0 && p <= 7) app->settings.backlight_mode = (uint8_t)p;
        } else if(!strcmp(key, "INFINITY_BACKLIGHT")) {
            sscanf((char*)(file_buf + line_end), "INFINITY_BACKLIGHT %d", &p);
            app->settings.backlight_mode = p ? 7 : 1;
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
        } else if(!strcmp(key, "CO2_TYPE")) {
            sscanf((char*)(file_buf + line_end), "\nCO2_TYPE %d", &p);
            app->settings.co2_type = (Co2SensorType)p;
        } else if(!strcmp(key, "CLIMATE_TYPE_IDX")) {
            sscanf((char*)(file_buf + line_end), "\nCLIMATE_TYPE_IDX %d", &p);
            app->settings.climate_type_idx = (uint8_t)p;
        } else if(!strcmp(key, "LED_NOTIFY")) {
            sscanf((char*)(file_buf + line_end), "\nLED_NOTIFY %d", &p);
            app->settings.led_notify = (bool)p;
        } else if(!strcmp(key, "SOUND_NOTIFY")) {
            sscanf((char*)(file_buf + line_end), "\nSOUND_NOTIFY %d", &p);
            app->settings.sound_notify = (bool)p;
        } else if(!strcmp(key, "SOUND_VOLUME")) {
            sscanf((char*)(file_buf + line_end), "\nSOUND_VOLUME %d", &p);
            app->settings.sound_volume = (uint8_t)p;
        } else if(!strcmp(key, "CO2_ALERT_THRESHOLD")) {
            sscanf((char*)(file_buf + line_end), "\nCO2_ALERT_THRESHOLD %d", &p);
            app->settings.co2_alert_threshold = (uint16_t)p;
        } else if(!strcmp(key, "CO2_PWM_RANGE")) {
            sscanf((char*)(file_buf + line_end), "\nCO2_PWM_RANGE %d", &p);
            if(p >= 2000 && p <= 10000) app->settings.co2_pwm_range = (uint16_t)p;
        } else if(!strcmp(key, "DEBUG_MODE")) {
            sscanf((char*)(file_buf + line_end), "\nDEBUG_MODE %d", &p);
            app->settings.debug_mode = (bool)p;
        } else if(!strcmp(key, "SHOW_STATUS")) {
            sscanf((char*)(file_buf + line_end), "\nSHOW_STATUS %d", &p);
            app->settings.show_status = (bool)p;
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

/* ---- Backlight: direct HAL control ---- */

static const uint32_t bl_minutes[] = {0, 0, 1, 5, 10, 20, 60, 0};

void air_stats_apply_backlight(void) {
    uint8_t m = app->settings.backlight_mode;
    app->backlight_deadline = 0;
    if(m == 0) {
        /* Off: HAL off */
        furi_hal_light_set(LightBacklight, 0);
    } else if(m == 1) {
        /* Auto: system controls */
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    } else if(m == 7) {
        /* Inf: HAL on */
        furi_hal_light_set(LightBacklight, 0xFF);
    } else {
        /* Timed: HAL on + set deadline */
        furi_hal_light_set(LightBacklight, 0xFF);
        app->backlight_deadline = furi_get_tick() + furi_ms_to_ticks(bl_minutes[m] * 60000);
    }
}

void air_stats_backlight_activity(void) {
    uint8_t m = app->settings.backlight_mode;
    if(m >= 2 && m <= 6) {
        /* Timed: turn on + reset deadline */
        furi_hal_light_set(LightBacklight, 0xFF);
        app->backlight_deadline = furi_get_tick() + furi_ms_to_ticks(bl_minutes[m] * 60000);
    }
}

/* ---- Tick callback: poll all sensors ---- */

static void tick_callback(void* context) {
    UNUSED(context);

    /* Backlight: direct HAL enforcement, cached + throttled */
    {
        static uint8_t bl_last = 0xFF;
        static uint32_t bl_last_tick = 0;
        uint8_t bl_want = 0xFF;
        uint8_t m = app->settings.backlight_mode;

        if(m == 0) {
            bl_want = 0;
        } else if(m == 1) {
            bl_want = 0xFF; /* marker: don't touch */
        } else if(m == 7) {
            bl_want = 0xFF;
        } else {
            if(app->backlight_deadline > 0 && furi_get_tick() >= app->backlight_deadline) {
                app->backlight_deadline = 0;
            }
            bl_want = (app->backlight_deadline > 0) ? 0xFF : 0;
        }

        if(m != 1) { /* Auto: skip — system controls */
            uint32_t now = furi_get_tick();
            bool changed = (bl_want != bl_last);
            bool throttle_ok = (now - bl_last_tick) >= 5000;
            if(changed || throttle_ok) {
                furi_hal_light_set(LightBacklight, bl_want);
                bl_last = bl_want;
                bl_last_tick = now;
            }
        }
    }

    if(app->sensors_update) {
        app->sensors_update = false;
        unitemp_sensors_deInit();
        unitemp_sensors_init();
        app->sensors_ready = true;
    } else if(app->sensors_ready) {
        unitemp_sensors_updateValues();
        air_stats_update_led();
        air_stats_check_sound_alert();
    }
}

/* ---- Application entry point ---- */

int32_t air_stats_main(void* p) {
    UNUSED(p);

    /* Allocate and zero-init app struct */
    app = malloc(sizeof(App));
    furi_check(app != NULL);
    memset(app, 0, sizeof(App));

    /* Open system records */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    /* Default settings (applied before loadSettings; loadSettings overwrites from file) */
    app->settings.backlight_mode = 7; /* Inf */
    app->settings.temp_unit = UT_TEMP_CELSIUS;
    app->settings.humidity_unit = UT_HUMIDITY_RELATIVE;
    app->settings.pressure_unit = UT_PRESSURE_MM_HG;
    app->settings.heat_index = false;
    app->settings.lastOTGState = furi_hal_power_is_otg_enabled();
    app->settings.co2_type = CO2_TYPE_PWM;
    app->settings.climate_type_idx = 0;
    app->settings.led_notify = true;
    app->settings.sound_notify = true;
    app->settings.sound_volume = 5;
    app->settings.co2_alert_threshold = 1000;
    app->settings.co2_pwm_range = 5000;
    app->settings.debug_mode = false;
    app->settings.show_status = true;

    /* Load settings from SD (creates defaults if missing) */
    unitemp_loadSettings();

    /* Apply backlight setting */
    air_stats_apply_backlight();

    /* ViewDispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);

    /* Popup (registered as ViewPopup in dispatcher) */
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewPopup, popup_get_view(app->popup));

    /* Allocate views (need app->view_dispatcher already set) */
    view_main_alloc();
    view_main_menu_alloc();
    view_settings_alloc();
    view_sensor_edit_alloc();
    view_sensor_actions_alloc();
    view_sensor_info_alloc();
    view_widgets_alloc();

    /* Sensors: load from SD card, fallback to hardcoded defaults */
    app->sensors = NULL;
    app->sensors_count = 0;

    if(!unitemp_sensors_load() || app->sensors_count == 0) {
        /* Default: BME280 (I2C addr 0xEC = 0x76<<1) + MH-Z19C (PWM PA6) */
        Sensor* bme = unitemp_sensor_alloc("BME280", &BME280, "EC");
        if(bme) {
            bme->temp_offset = -20; /* -2.0°C: compensate self-heating */
            unitemp_sensors_add(bme);
        }
        Sensor* co2 = unitemp_sensor_alloc("MH-Z19C", &MHZ19C, "");
        if(co2) unitemp_sensors_add(co2);
        unitemp_sensors_save(); /* persist defaults */
    }

    /* Sync CO2 sensor type: settings.co2_type must match the actual loaded sensor.
     * If they diverge (e.g. save was interrupted last session), fix silently. */
    {
        const SensorType* expected = (app->settings.co2_type == CO2_TYPE_UART) ? &MHZ19C_UART :
                                                                                 &MHZ19C;
        bool ok = false;
        for(uint8_t i = 0; i < app->sensors_count; i++) {
            if(app->sensors[i]->type == expected) {
                ok = true;
                break;
            }
        }
        if(!ok) {
            for(uint8_t i = 0; i < app->sensors_count; i++) {
                if(app->sensors[i]->type->datatype & UT_CO2)
                    app->sensors[i]->status = UT_SENSORSTATUS_INACTIVE;
            }
            Sensor* co2 = unitemp_sensor_alloc("MH-Z19C", expected, "");
            if(co2) unitemp_sensors_add(co2);
            unitemp_sensors_save();
        }
    }

    /* Initialize sensors (enables OTG power, sets up GPIO) */
    unitemp_sensors_init();

    /* CO2 alert runtime state */
    app->co2_was_above = false;
    app->last_alert_tick = 0;

    /* Set up 100 ms polling tick */
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, tick_callback, furi_ms_to_ticks(20));

    /* Attach dispatcher to GUI and run */
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_main_switch();
    view_dispatcher_run(app->view_dispatcher);

    /* --- Cleanup --- */

    /* Restore backlight to system */
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

    /* Turn off LED on exit */
    furi_hal_light_set(LightRed | LightGreen | LightBlue, 0);

    unitemp_sensors_deInit();
    unitemp_sensors_free();

    view_widgets_free();
    view_sensor_info_free();
    view_sensor_actions_free();
    view_sensor_edit_free();
    view_settings_free();
    view_main_menu_free();
    view_main_free();

    view_dispatcher_remove_view(app->view_dispatcher, ViewPopup);
    popup_free(app->popup);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
    app = NULL;

    return 0;
}
