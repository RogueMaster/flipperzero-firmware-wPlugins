#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <math.h>

#define MQ3_ADC_CHANNEL FuriHalAdcChannel11
#define MQ3_HEATER_PIN  &gpio_ext_pa7

typedef enum {
    AppStateHeaterOff,
    AppStateWarming,
    AppStateMeasuring,
    AppStateResult,
    AppStateDebug,
    AppStateViewLog,
    AppStateClearLogConfirm,
} AppState;

typedef enum {
    LevelSober,
    LevelLight,
    LevelMedium,
    LevelHeavy,
    LevelCritical,
} IntoxicationLevel;

#define THRESHOLD_SOBER  0.16f
#define THRESHOLD_LIGHT  0.5f
#define THRESHOLD_MEDIUM 1.0f
#define THRESHOLD_HEAVY  2.0f

#define MIN_WARMUP_TIME          30000
#define CALIBRATION_START_OFFSET 5000
#define MEASUREMENT_DURATION     10000
#define HEATER_TIMEOUT           60000

#define VOLTAGE_BUFFER_SIZE 20
#define MGL_EMA_ALPHA       0.15f

#define VC_ACTUAL 4.4f
#define R1_KOHM   10.0f
#define R2_KOHM   10.0f

#define R0_MIN 30.0f
#define R0_MAX 250.0f

#define DEBUG_PRESS_COUNT   3
#define DEBUG_PRESS_TIMEOUT 1000

#define LOG_FILE_PATH      "/ext/apps_data/alcohol_sensor/measuring.log"
#define LOG_LINES_PER_PAGE 4
#define LOG_LINE_HEIGHT    10

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    AppState state;
    AppState previous_state;
    AppState return_from_log_state;
    uint32_t warmup_start_time;
    uint32_t measurement_start_time;
    uint32_t last_measurement_end_time;
    bool heater_on;
    float current_mgl;
    float current_voltage;
    float current_mgl_ema;
    float peak_mgl;
    uint32_t peak_timestamp;
    uint32_t last_measure_time;
    bool running;
    int last_alert_level;
    IntoxicationLevel intoxication_level;
    float r0_ema;
    float v_clean_ema;
    bool calibrated;
    float voltage_buffer[VOLTAGE_BUFFER_SIZE];
    uint8_t voltage_buffer_index;
    uint32_t valid_measurements_count;
    uint8_t right_press_count;
    uint32_t last_right_press_time;
    bool clear_log_selection;
    int32_t log_scroll_offset;
    int32_t log_total_lines;
    bool log_loaded;
    FuriString** log_lines;
    int32_t log_lines_count;
} Mq3App;

static void mq3_app_draw_callback(Canvas* canvas, void* context);
static void mq3_app_input_callback(InputEvent* input_event, void* context);
static void check_and_play_alert(Mq3App* app, float mgl);
static void save_to_log(float mgl);
static void clear_log(void);
static void load_log(Mq3App* app);
static void free_log(Mq3App* app);
static void heater_on(Mq3App* app);
static void heater_off(Mq3App* app);
static void check_heater_timeout(Mq3App* app);

static void heater_on(Mq3App* app) {
    if(!app || app->heater_on) return;
    furi_hal_gpio_init(MQ3_HEATER_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(MQ3_HEATER_PIN, true);
    app->heater_on = true;
}

static void heater_off(Mq3App* app) {
    if(!app || !app->heater_on) return;
    furi_hal_gpio_write(MQ3_HEATER_PIN, false);
    furi_hal_gpio_init(MQ3_HEATER_PIN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    app->heater_on = false;
}

static void check_heater_timeout(Mq3App* app) {
    if(!app || !app->heater_on) return;
    if(app->state == AppStateWarming || app->state == AppStateMeasuring) return;
    uint32_t now = furi_get_tick();
    if(now - app->last_measurement_end_time >= HEATER_TIMEOUT) {
        heater_off(app);
    }
}

static IntoxicationLevel get_intoxication_level(float mgl) {
    if(mgl < THRESHOLD_SOBER) return LevelSober;
    if(mgl < THRESHOLD_LIGHT) return LevelLight;
    if(mgl < THRESHOLD_MEDIUM) return LevelMedium;
    if(mgl < THRESHOLD_HEAVY) return LevelHeavy;
    return LevelCritical;
}

static const char* get_level_name(IntoxicationLevel level) {
    switch(level) {
    case LevelSober:
        return "Sober";
    case LevelLight:
        return "Light";
    case LevelMedium:
        return "Medium";
    case LevelHeavy:
        return "Heavy";
    case LevelCritical:
        return "CRITICAL!";
    default:
        return "";
    }
}

static const char* get_warmup_quality(Mq3App* app) {
    if(!app) return "";
    uint32_t elapsed = furi_get_tick() - app->warmup_start_time;
    float ratio = (float)elapsed / (float)MIN_WARMUP_TIME;
    if(ratio < 0.2f) return "Cold";
    if(ratio < 0.5f) return "Warming";
    if(ratio < 0.8f) return "Ready";
    return "Optimal";
}

static const char* get_state_name(AppState state) {
    switch(state) {
    case AppStateHeaterOff:
        return "Heater Off";
    case AppStateWarming:
        return "Warming";
    case AppStateMeasuring:
        return "Measuring";
    case AppStateResult:
        return "Result";
    case AppStateDebug:
        return "Debug";
    case AppStateViewLog:
        return "Log";
    case AppStateClearLogConfirm:
        return "Clear?";
    default:
        return "Unknown";
    }
}

static void load_log(Mq3App* app) {
    if(!app) return;
    free_log(app);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, LOG_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        app->log_total_lines = 0;
        FuriString* temp_line = furi_string_alloc();
        while(stream_read_line(stream, temp_line)) {
            app->log_total_lines++;
        }
        furi_string_free(temp_line);
        if(app->log_total_lines > 0) {
            app->log_lines = malloc(app->log_total_lines * sizeof(FuriString*));
            stream_rewind(stream);
            for(int32_t i = 0; i < app->log_total_lines; i++) {
                app->log_lines[i] = furi_string_alloc();
                stream_read_line(stream, app->log_lines[i]);
            }
            app->log_lines_count = app->log_total_lines;
        }
        app->log_scroll_offset = 0;
        app->log_loaded = true;
    } else {
        app->log_total_lines = 0;
        app->log_lines = NULL;
        app->log_lines_count = 0;
        app->log_scroll_offset = 0;
        app->log_loaded = false;
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

static void free_log(Mq3App* app) {
    if(!app || !app->log_lines) return;
    for(int32_t i = 0; i < app->log_lines_count; i++) {
        if(app->log_lines[i]) furi_string_free(app->log_lines[i]);
    }
    free(app->log_lines);
    app->log_lines = NULL;
    app->log_lines_count = 0;
    app->log_total_lines = 0;
    app->log_loaded = false;
}

static void save_to_log(float mgl) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        storage_common_mkdir(storage, "/ext/apps_data/alcohol_sensor");
        FuriString* log_line = furi_string_alloc();
        Stream* stream = file_stream_alloc(storage);
        if(file_stream_open(stream, LOG_FILE_PATH, FSAM_READ_WRITE, FSOM_OPEN_APPEND)) {
            DateTime dt;
            furi_hal_rtc_get_datetime(&dt);
            furi_string_printf(
                log_line,
                "%02d.%02d.%02d %02d:%02d - %.3f mg/l\n",
                dt.day,
                dt.month,
                dt.year % 100,
                dt.hour,
                dt.minute,
                (double)mgl);
            stream_write_string(stream, log_line);
        }
        file_stream_close(stream);
        stream_free(stream);
        furi_string_free(log_line);
        furi_record_close(RECORD_STORAGE);
    }
}

static void clear_log(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        storage_common_mkdir(storage, "/ext/apps_data/alcohol_sensor");
        storage_common_remove(storage, LOG_FILE_PATH);
        furi_record_close(RECORD_STORAGE);
    }
}

static void
    draw_intoxication_scale(Canvas* canvas, IntoxicationLevel level, uint8_t x, uint8_t y) {
    const uint8_t steps = 5;
    const uint8_t step_width = 21;
    const uint8_t step_height = 14;
    const uint8_t gap = 3;
    const char* labels[] = {"S", "L", "M", "H", "C"};

    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < steps; i++) {
        uint8_t step_x = x + i * (step_width + gap);
        if(i <= level) {
            canvas_draw_box(canvas, step_x, y, step_width, step_height);
            canvas_set_color(canvas, ColorWhite);
            uint8_t text_width = canvas_string_width(canvas, labels[i]);
            canvas_draw_str(
                canvas, step_x + (step_width - text_width) / 2, y + step_height - 3, labels[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_frame(canvas, step_x, y, step_width, step_height);
            uint8_t text_width = canvas_string_width(canvas, labels[i]);
            canvas_draw_str(
                canvas, step_x + (step_width - text_width) / 2, y + step_height - 3, labels[i]);
        }
    }
}

static void draw_debug_screen(Canvas* canvas, Mq3App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 12, 9, "DEBUG INFO");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    canvas_set_font(canvas, FontSecondary);
    char debug_str[32];

    snprintf(debug_str, sizeof(debug_str), "State: %s", get_state_name(app->previous_state));
    canvas_draw_str(canvas, 2, 22, debug_str);
    snprintf(debug_str, sizeof(debug_str), "V: %.3fV", (double)app->current_voltage);
    canvas_draw_str(canvas, 2, 31, debug_str);
    snprintf(debug_str, sizeof(debug_str), "Z: %.3fV", (double)app->v_clean_ema);
    canvas_draw_str(canvas, 2, 40, debug_str);
    snprintf(debug_str, sizeof(debug_str), "R0: %.1fk", (double)app->r0_ema);
    canvas_draw_str(canvas, 2, 49, debug_str);
    snprintf(debug_str, sizeof(debug_str), "Q: %s", get_warmup_quality(app));
    canvas_draw_str(canvas, 2, 58, debug_str);
    snprintf(debug_str, sizeof(debug_str), "Heater: %s", app->heater_on ? "ON" : "OFF");
    canvas_draw_str(canvas, 72, 22, debug_str);
    snprintf(debug_str, sizeof(debug_str), "Cal: %s", app->calibrated ? "Yes" : "No");
    canvas_draw_str(canvas, 72, 31, debug_str);
    snprintf(
        debug_str, sizeof(debug_str), "Cnt: %lu", (unsigned long)app->valid_measurements_count);
    canvas_draw_str(canvas, 72, 40, debug_str);
    snprintf(debug_str, sizeof(debug_str), "Peak: %.3f", (double)app->peak_mgl);
    canvas_draw_str(canvas, 72, 49, debug_str);
    snprintf(debug_str, sizeof(debug_str), "Raw: %.3f", (double)app->current_mgl_ema);
    canvas_draw_str(canvas, 72, 58, debug_str);
    canvas_draw_str(canvas, 22, 64, "BACK - Return");
}

static void draw_log_screen(Canvas* canvas, Mq3App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 9, "MEASUREMENT LOG");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    canvas_set_font(canvas, FontSecondary);

    if(!app->log_loaded || app->log_total_lines == 0) {
        canvas_draw_str(canvas, 10, 30, "No measurements yet");
    } else {
        int32_t start_line = app->log_total_lines - LOG_LINES_PER_PAGE - app->log_scroll_offset;
        if(start_line < 0) start_line = 0;

        uint8_t y_pos = 22;
        for(int32_t i = 0; i < LOG_LINES_PER_PAGE; i++) {
            int32_t line_index = start_line + i;
            if(line_index < app->log_total_lines && line_index >= 0) {
                canvas_draw_str(
                    canvas, 2, y_pos, furi_string_get_cstr(app->log_lines[line_index]));
            }
            y_pos += LOG_LINE_HEIGHT;
        }

        if(app->log_total_lines > LOG_LINES_PER_PAGE) {
            float scroll_ratio =
                (float)app->log_scroll_offset / (float)(app->log_total_lines - LOG_LINES_PER_PAGE);
            if(scroll_ratio > 1.0f) scroll_ratio = 1.0f;
            if(scroll_ratio < 0.0f) scroll_ratio = 0.0f;

            uint8_t scroll_y =
                22 + (uint8_t)(scroll_ratio * (LOG_LINES_PER_PAGE * LOG_LINE_HEIGHT - 4));
            canvas_draw_frame(canvas, 126, 22, 2, LOG_LINES_PER_PAGE * LOG_LINE_HEIGHT);
            canvas_draw_box(canvas, 126, scroll_y, 2, 4);
        }
    }

    canvas_draw_str(canvas, 2, 62, "Up/Dn-scroll");
    canvas_draw_str(canvas, 80, 62, "OK-Clr");
}

static void draw_clear_confirm_screen(Canvas* canvas, Mq3App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 20, "CLEAR LOG?");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 20, 40, "All records will be");
    canvas_draw_str(canvas, 25, 52, "permanently deleted");

    if(app->clear_log_selection) {
        canvas_draw_str(canvas, 15, 64, "<- Yes   No ->");
    } else {
        canvas_draw_str(canvas, 15, 64, "Yes   <- No ->");
    }
}

static void check_and_play_alert(Mq3App* app, float mgl) {
    if(!app || app->state != AppStateMeasuring) return;

    IntoxicationLevel current_level = get_intoxication_level(mgl);

    if(current_level > app->last_alert_level) {
        switch(current_level) {
        case LevelLight:
            notification_message(app->notifications, &sequence_set_vibro_on);
            furi_delay_ms(200);
            notification_message(app->notifications, &sequence_reset_vibro);
            break;
        case LevelMedium:
            notification_message(app->notifications, &sequence_set_vibro_on);
            for(int i = 0; i < 2; i++)
                furi_delay_ms(150);
            notification_message(app->notifications, &sequence_reset_vibro);
            break;
        case LevelHeavy:
            notification_message(app->notifications, &sequence_set_vibro_on);
            for(int i = 0; i < 3; i++)
                furi_delay_ms(200);
            notification_message(app->notifications, &sequence_reset_vibro);
            break;
        case LevelCritical:
            notification_message(app->notifications, &sequence_set_vibro_on);
            for(int i = 0; i < 5; i++)
                furi_delay_ms(200);
            notification_message(app->notifications, &sequence_reset_vibro);
            break;
        default:
            break;
        }
    }
    app->last_alert_level = current_level;
}

static float add_to_voltage_buffer(Mq3App* app, float voltage) {
    if(!app) return 0.0f;

    app->voltage_buffer[app->voltage_buffer_index] = voltage;
    app->voltage_buffer_index = (app->voltage_buffer_index + 1) % VOLTAGE_BUFFER_SIZE;

    float sum = 0.0f;
    int count = 0;
    for(int i = 0; i < VOLTAGE_BUFFER_SIZE; i++) {
        if(app->voltage_buffer[i] > 0.05f) {
            sum += app->voltage_buffer[i];
            count++;
        }
    }

    if(count > 0) return sum / (float)count;
    return voltage;
}

static void update_zero_calibration(Mq3App* app, float voltage) {
    if(!app || voltage < 0.1f) return;

    float alpha = 0.05f;

    if(app->v_clean_ema < 0.1f) {
        app->v_clean_ema = voltage;
    } else {
        app->v_clean_ema = alpha * voltage + (1.0f - alpha) * app->v_clean_ema;
    }

    float r0_new = (VC_ACTUAL * R2_KOHM / app->v_clean_ema) - R1_KOHM - R2_KOHM;
    if(r0_new < R0_MIN) r0_new = R0_MIN;
    if(r0_new > R0_MAX) r0_new = R0_MAX;

    app->r0_ema = r0_new;
    app->calibrated = true;
}

static float mq3_voltage_to_mgl(Mq3App* app, float current_voltage) {
    if(!app || !app->calibrated) return 0.0f;
    if(current_voltage < 0.1f) return 0.0f;

    float vout = current_voltage;
    float v_max = 2.048f;
    float mgl_max = 10.0f;

    if(vout <= app->v_clean_ema * 1.01f || (vout - app->v_clean_ema) < 0.01f) return 0.0f;
    if(vout >= v_max) return mgl_max;

    float rs = (VC_ACTUAL * R2_KOHM / vout) - R1_KOHM - R2_KOHM;
    if(rs < 1.0f) rs = 1.0f;
    if(rs > 1000.0f) rs = 1000.0f;

    float ratio = rs / app->r0_ema;
    if(ratio <= 0.01f) ratio = 0.01f;
    if(ratio >= 10.0f) ratio = 10.0f;

    float log_ratio = log10f(ratio);
    float log_ppm = (log_ratio - 0.62f) / -0.66f;
    float ppm = powf(10.0f, log_ppm);

    float mgl = ppm * 0.00188f;

    float ppm_clean = powf(10.0f, (0.0f - 0.62f) / -0.66f);
    float mgl_clean = ppm_clean * 0.00188f;

    float rs_at_max = (VC_ACTUAL * R2_KOHM / v_max) - R1_KOHM - R2_KOHM;
    float ratio_at_max = rs_at_max / app->r0_ema;
    float log_ratio_max = log10f(ratio_at_max);
    float log_ppm_max = (log_ratio_max - 0.62f) / -0.66f;
    float ppm_at_max = powf(10.0f, log_ppm_max);
    float mgl_at_max = ppm_at_max * 0.00188f;

    float mgl_scaled = ((mgl - mgl_clean) / (mgl_at_max - mgl_clean)) * mgl_max;
    if(mgl_scaled < 0.0f) mgl_scaled = 0.0f;
    if(mgl_scaled > mgl_max) mgl_scaled = mgl_max;

    return mgl_scaled;
}

static void mq3_perform_measurement(Mq3App* app) {
    if(!app) return;

    FuriHalAdcHandle* adc_handle = furi_hal_adc_acquire();
    if(adc_handle == NULL) return;

    furi_hal_adc_configure(adc_handle);

    uint32_t sum = 0;
    int valid_samples = 0;
    for(int i = 0; i < 20; i++) {
        uint16_t raw = furi_hal_adc_read(adc_handle, MQ3_ADC_CHANNEL);
        uint32_t voltage_mv = furi_hal_adc_convert_to_voltage(adc_handle, raw);
        float voltage = (float)voltage_mv / 1000.0f;

        if(voltage > 0.05f) {
            sum += raw;
            valid_samples++;
        }
        furi_delay_ms(5);
    }

    furi_hal_adc_release(adc_handle);

    if(valid_samples == 0) {
        app->current_voltage = 0.0f;
        app->current_mgl = 0.0f;
        return;
    }

    uint16_t avg_raw = sum / valid_samples;

    adc_handle = furi_hal_adc_acquire();
    if(adc_handle != NULL) {
        furi_hal_adc_configure(adc_handle);
        uint32_t voltage_mv = furi_hal_adc_convert_to_voltage(adc_handle, avg_raw);
        furi_hal_adc_release(adc_handle);

        float raw_voltage = (float)voltage_mv / 1000.0f;
        app->current_voltage = add_to_voltage_buffer(app, raw_voltage);
    } else {
        app->current_voltage = 0.0f;
        return;
    }

    app->valid_measurements_count++;

    if(app->state == AppStateWarming) {
        uint32_t elapsed = furi_get_tick() - app->warmup_start_time;
        if(elapsed >= (MIN_WARMUP_TIME - CALIBRATION_START_OFFSET) && !app->calibrated) {
            update_zero_calibration(app, app->current_voltage);
        }
    }

    if(app->state == AppStateMeasuring && app->calibrated &&
       app->current_voltage < app->v_clean_ema && app->current_voltage > 0.1f) {
        if(app->current_voltage < app->v_clean_ema * 0.95f) {
            app->calibrated = false;
            app->v_clean_ema = 0.0f;
            app->valid_measurements_count = 0;
        } else {
            app->v_clean_ema = 0.2f * app->current_voltage + 0.8f * app->v_clean_ema;
            float r0_new = (VC_ACTUAL * R2_KOHM / app->v_clean_ema) - R1_KOHM - R2_KOHM;
            if(r0_new < R0_MIN) r0_new = R0_MIN;
            if(r0_new > R0_MAX) r0_new = R0_MAX;
            app->r0_ema = r0_new;
        }
    }

    float new_mgl = mq3_voltage_to_mgl(app, app->current_voltage);

    if(app->current_mgl_ema < 0.001f) {
        app->current_mgl_ema = new_mgl;
    } else {
        app->current_mgl_ema =
            MGL_EMA_ALPHA * new_mgl + (1.0f - MGL_EMA_ALPHA) * app->current_mgl_ema;
    }

    app->current_mgl = app->current_mgl_ema;

    if(app->current_mgl > app->peak_mgl) {
        app->peak_mgl = app->current_mgl;
        app->peak_timestamp = furi_get_tick();
    }

    if(app->state == AppStateMeasuring) {
        app->intoxication_level = get_intoxication_level(app->current_mgl);
        check_and_play_alert(app, app->current_mgl);
    }

    app->last_measure_time = furi_get_tick();
}

static void mq3_app_draw_callback(Canvas* canvas, void* context) {
    Mq3App* app = (Mq3App*)context;
    if(!app || !canvas) return;

    if(app->state == AppStateDebug) {
        draw_debug_screen(canvas, app);
        return;
    }

    if(app->state == AppStateViewLog) {
        draw_log_screen(canvas, app);
        return;
    }

    if(app->state == AppStateClearLogConfirm) {
        draw_clear_confirm_screen(canvas, app);
        return;
    }

    canvas_clear(canvas);

    switch(app->state) {
    case AppStateHeaterOff:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 12, "MQ-3 Alcohol Meter");
        draw_intoxication_scale(canvas, LevelSober, 5, 24);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 30, 50, "OK - Start");
        canvas_draw_str(canvas, 10, 62, "Left-Log BACK-Exit");
        break;

    case AppStateWarming: {
        uint32_t elapsed = furi_get_tick() - app->warmup_start_time;
        uint32_t remaining = 0;
        uint32_t progress = 0;

        if(elapsed < MIN_WARMUP_TIME) {
            remaining = (MIN_WARMUP_TIME - elapsed) / 1000;
            progress = (elapsed * 100) / MIN_WARMUP_TIME;
        } else {
            progress = 100;
        }
        if(progress > 100) progress = 100;

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 20, 10, "Warming up...");
        canvas_set_font(canvas, FontSecondary);
        char buffer[24];
        if(remaining > 0) {
            snprintf(buffer, sizeof(buffer), "Wait: %lus", remaining);
        } else {
            snprintf(buffer, sizeof(buffer), "Ready");
        }
        canvas_draw_str(canvas, 35, 24, buffer);
        snprintf(buffer, sizeof(buffer), "%s", get_warmup_quality(app));
        canvas_draw_str(canvas, 38, 36, buffer);
        canvas_draw_frame(canvas, 14, 44, 100, 8);
        canvas_draw_box(canvas, 14, 44, progress, 8);
        canvas_draw_str(canvas, 10, 62, "OK-Start BACK-Exit");
        break;
    }

    case AppStateMeasuring: {
        uint32_t elapsed = furi_get_tick() - app->measurement_start_time;
        uint32_t progress = 0;

        if(elapsed < MEASUREMENT_DURATION) {
            progress = (elapsed * 100) / MEASUREMENT_DURATION;
        } else {
            progress = 100;
        }
        if(progress > 100) progress = 100;

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 25, 10, "Measuring...");
        draw_intoxication_scale(canvas, app->intoxication_level, 5, 18);
        canvas_set_font(canvas, FontPrimary);
        char mgl_str[20];
        snprintf(mgl_str, sizeof(mgl_str), "%.3f mg/l", (double)app->current_mgl);
        uint8_t mgl_width = canvas_string_width(canvas, mgl_str);
        canvas_draw_str(canvas, (128 - mgl_width) / 2, 44, mgl_str);
        canvas_set_font(canvas, FontSecondary);
        const char* status = get_level_name(app->intoxication_level);
        uint8_t status_width = canvas_string_width(canvas, status);
        canvas_draw_str(canvas, (128 - status_width) / 2, 54, status);
        canvas_draw_frame(canvas, 14, 62, 100, 4);
        canvas_draw_box(canvas, 14, 62, progress, 4);
        uint8_t dots = (elapsed / 250) % 4;
        for(uint8_t i = 0; i < dots; i++) {
            canvas_draw_dot(canvas, 100 + i * 4, 8);
        }
        break;
    }

    case AppStateResult: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 25, 10, "Result");
        draw_intoxication_scale(canvas, app->intoxication_level, 5, 15);
        canvas_set_font(canvas, FontPrimary);
        char mgl_str[24];
        snprintf(mgl_str, sizeof(mgl_str), "Peak: %.3f mg/l", (double)app->peak_mgl);
        uint8_t mgl_width = canvas_string_width(canvas, mgl_str);
        canvas_draw_str(canvas, (128 - mgl_width) / 2, 38, mgl_str);
        canvas_set_font(canvas, FontSecondary);
        const char* status = get_level_name(app->intoxication_level);
        char sober_time[32];
        float mgl = app->peak_mgl;

        if(mgl < THRESHOLD_SOBER) {
            snprintf(sober_time, sizeof(sober_time), "Sober - You're fine!");
        } else if(mgl < THRESHOLD_LIGHT) {
            int minutes = (int)(30 + (mgl - THRESHOLD_SOBER) * 180);
            if(minutes < 60) {
                snprintf(sober_time, sizeof(sober_time), "%s - ~%d min", status, minutes);
            } else {
                snprintf(
                    sober_time,
                    sizeof(sober_time),
                    "%s - ~%dh %dm",
                    status,
                    minutes / 60,
                    minutes % 60);
            }
        } else if(mgl < THRESHOLD_MEDIUM) {
            int minutes = (int)(90 + (mgl - THRESHOLD_LIGHT) * 180);
            snprintf(
                sober_time,
                sizeof(sober_time),
                "%s - ~%dh %dm",
                status,
                minutes / 60,
                minutes % 60);
        } else if(mgl < THRESHOLD_HEAVY) {
            int minutes = (int)(180 + (mgl - THRESHOLD_MEDIUM) * 180);
            snprintf(
                sober_time,
                sizeof(sober_time),
                "%s - ~%dh %dm",
                status,
                minutes / 60,
                minutes % 60);
        } else {
            int minutes = (int)(360 + (mgl - THRESHOLD_HEAVY) * 120);
            if(minutes > 720) minutes = 720;
            snprintf(
                sober_time,
                sizeof(sober_time),
                "%s - ~%dh %dm",
                status,
                minutes / 60,
                minutes % 60);
        }

        uint8_t sober_width = canvas_string_width(canvas, sober_time);
        canvas_draw_str(canvas, (128 - sober_width) / 2, 49, sober_time);
        canvas_draw_str(canvas, 5, 62, "OK-New L-Log B-Exit");
        break;
    }

    case AppStateDebug:
    case AppStateViewLog:
    case AppStateClearLogConfirm:
        break;
    }
}

static void mq3_app_input_callback(InputEvent* input_event, void* context) {
    Mq3App* app = (Mq3App*)context;
    if(!app || !input_event) return;

    if(input_event->type == InputTypeShort) {
        if(app->state == AppStateDebug) {
            if(input_event->key == InputKeyBack) {
                app->state = app->previous_state;
                notification_message(app->notifications, &sequence_single_vibro);
            }
            return;
        }

        if(app->state == AppStateViewLog) {
            if(input_event->key == InputKeyBack) {
                free_log(app);
                app->state = app->return_from_log_state;
                notification_message(app->notifications, &sequence_single_vibro);
            } else if(input_event->key == InputKeyUp) {
                if(app->log_total_lines > LOG_LINES_PER_PAGE) {
                    app->log_scroll_offset++;
                    if(app->log_scroll_offset > app->log_total_lines - LOG_LINES_PER_PAGE) {
                        app->log_scroll_offset = app->log_total_lines - LOG_LINES_PER_PAGE;
                    }
                }
            } else if(input_event->key == InputKeyDown) {
                app->log_scroll_offset--;
                if(app->log_scroll_offset < 0) app->log_scroll_offset = 0;
            } else if(input_event->key == InputKeyOk) {
                app->state = AppStateClearLogConfirm;
                app->clear_log_selection = false;
            }
            return;
        }

        if(app->state == AppStateClearLogConfirm) {
            if(input_event->key == InputKeyBack) {
                app->state = AppStateViewLog;
                notification_message(app->notifications, &sequence_single_vibro);
            } else if(input_event->key == InputKeyLeft) {
                app->clear_log_selection = true;
            } else if(input_event->key == InputKeyRight) {
                app->clear_log_selection = false;
            } else if(input_event->key == InputKeyOk) {
                if(app->clear_log_selection) {
                    clear_log();
                    free_log(app);
                    load_log(app);
                }
                app->state = AppStateViewLog;
            }
            return;
        }

        if(input_event->key == InputKeyLeft) {
            if(app->state == AppStateHeaterOff || app->state == AppStateResult) {
                app->return_from_log_state = app->state;
                load_log(app);
                app->state = AppStateViewLog;
            }
            return;
        }

        if(input_event->key == InputKeyRight) {
            uint32_t current_time = furi_get_tick();
            if(current_time - app->last_right_press_time > DEBUG_PRESS_TIMEOUT) {
                app->right_press_count = 0;
            }
            app->right_press_count++;
            app->last_right_press_time = current_time;

            if(app->right_press_count >= DEBUG_PRESS_COUNT) {
                app->right_press_count = 0;
                app->previous_state = app->state;
                app->state = AppStateDebug;
            }
            return;
        }

        if(input_event->key == InputKeyBack) {
            if(app->state == AppStateHeaterOff) {
                app->running = false;
            } else if(
                app->state == AppStateResult || app->state == AppStateMeasuring ||
                app->state == AppStateWarming) {
                app->state = AppStateHeaterOff;
                app->last_measurement_end_time = furi_get_tick();
                app->peak_mgl = 0;
                app->peak_timestamp = 0;
                app->calibrated = false;
                app->current_mgl_ema = 0.0f;
                app->valid_measurements_count = 0;
            }
        } else if(input_event->key == InputKeyOk) {
            if(app->state == AppStateHeaterOff) {
                notification_message(app->notifications, &sequence_set_vibro_on);
                furi_delay_ms(200);
                notification_message(app->notifications, &sequence_reset_vibro);

                heater_on(app);
                app->warmup_start_time = furi_get_tick();
                app->state = AppStateWarming;
                app->peak_mgl = 0;
                app->peak_timestamp = 0;
                app->calibrated = false;
                app->current_mgl_ema = 0.0f;
                app->valid_measurements_count = 0;
                app->v_clean_ema = 0.0f;
                app->r0_ema = 68.0f;

                for(int i = 0; i < VOLTAGE_BUFFER_SIZE; i++) {
                    app->voltage_buffer[i] = 0.0f;
                }
                app->voltage_buffer_index = 0;
            } else if(app->state == AppStateWarming) {
                uint32_t elapsed = furi_get_tick() - app->warmup_start_time;
                if(elapsed >= MIN_WARMUP_TIME) {
                    notification_message(app->notifications, &sequence_set_vibro_on);
                    furi_delay_ms(200);
                    notification_message(app->notifications, &sequence_reset_vibro);

                    app->measurement_start_time = furi_get_tick();
                    app->state = AppStateMeasuring;
                    app->peak_mgl = 0;
                    app->peak_timestamp = 0;
                } else {
                    notification_message(app->notifications, &sequence_error);
                }
            } else if(app->state == AppStateResult) {
                notification_message(app->notifications, &sequence_set_vibro_on);
                furi_delay_ms(200);
                notification_message(app->notifications, &sequence_reset_vibro);

                heater_on(app);
                app->warmup_start_time = furi_get_tick();
                app->state = AppStateWarming;
                app->peak_mgl = 0;
                app->peak_timestamp = 0;
                app->calibrated = false;
                app->current_mgl_ema = 0.0f;
                app->valid_measurements_count = 0;
                app->v_clean_ema = 0.0f;
                app->r0_ema = 68.0f;
            }
        }
    }
}

int32_t alcohol_sensor_app(void* p) {
    UNUSED(p);

    Mq3App* app = malloc(sizeof(Mq3App));
    if(app == NULL) return -1;

    memset(app, 0, sizeof(Mq3App));
    app->running = true;
    app->last_measure_time = 0;
    app->last_alert_level = 0;
    app->warmup_start_time = 0;
    app->measurement_start_time = 0;
    app->last_measurement_end_time = 0;
    app->heater_on = false;
    app->peak_mgl = 0;
    app->peak_timestamp = 0;
    app->intoxication_level = LevelSober;
    app->state = AppStateHeaterOff;
    app->previous_state = AppStateHeaterOff;
    app->return_from_log_state = AppStateHeaterOff;
    app->calibrated = false;
    app->r0_ema = 68.0f;
    app->v_clean_ema = 0.0f;
    app->current_mgl_ema = 0.0f;
    app->valid_measurements_count = 0;
    app->voltage_buffer_index = 0;
    app->right_press_count = 0;
    app->last_right_press_time = 0;
    app->clear_log_selection = false;
    app->log_scroll_offset = 0;
    app->log_total_lines = 0;
    app->log_loaded = false;
    app->log_lines = NULL;
    app->log_lines_count = 0;

    for(int i = 0; i < VOLTAGE_BUFFER_SIZE; i++) {
        app->voltage_buffer[i] = 0.0f;
    }

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    if(!furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
    }

    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    view_port_draw_callback_set(app->view_port, mq3_app_draw_callback, app);
    view_port_input_callback_set(app->view_port, mq3_app_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    while(app->running) {
        uint32_t now = furi_get_tick();

        if(app->state == AppStateWarming || app->state == AppStateMeasuring ||
           app->state == AppStateDebug) {
            if(now - app->last_measure_time > 500) {
                mq3_perform_measurement(app);
            }
        }

        if(app->state == AppStateMeasuring) {
            uint32_t elapsed = now - app->measurement_start_time;
            if(elapsed >= MEASUREMENT_DURATION) {
                save_to_log(app->peak_mgl);
                app->state = AppStateResult;
                app->last_measurement_end_time = furi_get_tick();
                app->intoxication_level = get_intoxication_level(app->peak_mgl);
                notification_message(app->notifications, &sequence_success);
            }
        }

        if(app->state == AppStateHeaterOff || app->state == AppStateResult) {
            check_heater_timeout(app);
        }

        view_port_update(app->view_port);

        InputEvent event;
        while(furi_message_queue_get(app->event_queue, &event, 0) == FuriStatusOk) {
            mq3_app_input_callback(&event, app);
        }

        furi_delay_ms(50);
    }

    free_log(app);
    heater_off(app);

    if(furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);

    return 0;
}
