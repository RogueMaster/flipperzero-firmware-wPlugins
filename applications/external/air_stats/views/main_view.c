/*
 * main_view.c — main sensor display screen.
 * Sections are shown only when data is valid (collapsed otherwise).
 * OK button → main menu.  Back → exit app.
 */
#include "../air_stats_i.h"
#include <gui/elements.h>
#include <furi_hal_rtc.h>
#include <furi_hal_power.h>

static View* view;

#define VIEW_ID   ViewMain
#define SCREEN_W  128
#define CONTENT_H 52 /* 64px screen - 12px elements_button_center */

/* CO2 block: big number row + separator */
#define CO2_NUM_H   18 /* FontBigNumbers leading */
#define CO2_SEP_H   2 /* separator line + gap */
#define CO2_BLOCK_H (CO2_NUM_H + CO2_SEP_H)

/* Climate: one FontPrimary line */
#define CLIM_LINE_H 12

static const char* co2_quality(float co2) {
    if(co2 < 800.0f) return "GOOD"; /* green */
    if(co2 < 1000.0f) return "NORM"; /* yellow */
    if(co2 < 1400.0f) return "POOR"; /* orange */
    return "BAD"; /* red */
}

/* Check if PWM CO2 sensor is frozen (no edge for 5+ sec) */
static bool is_co2_frozen(void) {
    if(!app->sensors_ready || !app->sensors) return false;
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        Sensor* s = app->sensors[i];
        if(!s || s->status == UT_SENSORSTATUS_INACTIVE) continue;
        if((s->type->datatype & UT_CO2) && s->co2 > 0.0f && s->last_valid_tick > 0 &&
           (furi_get_tick() - s->last_valid_tick) > 5000) {
            return true;
        }
    }
    return false;
}

/* Find first active CO2 sensor */
static Sensor* find_co2_sensor(void) {
    if(!app->sensors_ready || !app->sensors) return NULL;
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        Sensor* s = app->sensors[i];
        if(!s || s->status == UT_SENSORSTATUS_INACTIVE) continue;
        if(s->type->datatype & UT_CO2) return s;
    }
    return NULL;
}

static void draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    elements_button_center(canvas, "Menu");
    if(is_co2_frozen()) {
        elements_button_left(canvas, "Eject");
    }

    if(!app->sensors_ready || !app->sensors) return;

    /* Find first CO2 and first climate sensor */
    Sensor* co2_sensor = NULL;
    Sensor* clim_sensor = NULL;
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        Sensor* s = app->sensors[i];
        if(!s || s->status == UT_SENSORSTATUS_INACTIVE) continue;
        if((s->type->datatype & UT_CO2) && !co2_sensor) co2_sensor = s;
        if(!(s->type->datatype & UT_CO2) && !clim_sensor) clim_sensor = s;
    }

    bool co2_valid = co2_sensor && co2_sensor->co2 > 0.0f;
    bool clim_ok = clim_sensor && clim_sensor->status == UT_SENSORSTATUS_OK;
    bool has_th = clim_ok && (clim_sensor->type->datatype & (UT_TEMPERATURE | UT_HUMIDITY));
    bool has_press = clim_ok && (clim_sensor->type->datatype & UT_PRESSURE);

    /* Nothing valid — show placeholder */
    if(!co2_valid && !has_th && !has_press) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 22, AlignCenter, AlignBottom, "No sensors");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 33, AlignCenter, AlignBottom, "Connect and configure");
        return;
    }

    /* Vertical centering: measure total content height */
    uint8_t total_h = 0;
    if(co2_valid) total_h += CO2_BLOCK_H;
    if(has_th) total_h += CLIM_LINE_H;
    if(has_press) total_h += CLIM_LINE_H;

    int16_t y = ((int16_t)CONTENT_H - total_h) / 2;
    if(y < 0) y = 0;

    char buf[32];

    /* CO2 block */
    if(co2_valid) {
        /* Number — FontBigNumbers, slightly left of center */
        canvas_set_font(canvas, FontBigNumbers);
        snprintf(buf, sizeof(buf), "%d", (int)co2_sensor->co2);
        canvas_draw_str_aligned(canvas, 54, y + CO2_NUM_H, AlignCenter, AlignBottom, buf);

        /* Freeze indicator — left edge */
        bool co2_frozen = co2_sensor->last_valid_tick > 0 &&
                          (furi_get_tick() - co2_sensor->last_valid_tick) > 5000;
        if(co2_frozen) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 1, y + CO2_NUM_H, AlignLeft, AlignBottom, "freeze");
        }

        /* Quality label — FontPrimary, right edge, same baseline */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas,
            SCREEN_W - 1,
            y + CO2_NUM_H,
            AlignRight,
            AlignBottom,
            co2_quality(co2_sensor->co2));

        y += CO2_NUM_H;
        canvas_draw_line(canvas, 0, (uint8_t)y, SCREEN_W - 1, (uint8_t)y);
        y += CO2_SEP_H;
    }

    /* Unit strings */
    const char* temp_unit_str = (app->settings.temp_unit == UT_TEMP_FAHRENHEIT) ? "F" : "C";
    const char* press_unit_str;
    switch(app->settings.pressure_unit) {
    case UT_PRESSURE_IN_HG:
        press_unit_str = "inHg";
        break;
    case UT_PRESSURE_KPA:
        press_unit_str = "kPa";
        break;
    case UT_PRESSURE_HPA:
        press_unit_str = "hPa";
        break;
    default:
        press_unit_str = "mmHg";
        break;
    }

    canvas_set_font(canvas, FontPrimary);

    /* Temperature + humidity row */
    if(has_th) {
        int pos = 0;
        if(clim_sensor->type->datatype & UT_TEMPERATURE)
            pos += snprintf(
                buf + pos, sizeof(buf) - pos, "%.1f*%s", (double)clim_sensor->temp, temp_unit_str);
        if(clim_sensor->type->datatype & UT_HUMIDITY) {
            if(pos) pos += snprintf(buf + pos, sizeof(buf) - pos, "   ");
            snprintf(buf + pos, sizeof(buf) - pos, "%.0f%%", (double)clim_sensor->hum);
        }
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, y + CLIM_LINE_H, AlignCenter, AlignBottom, buf);
        y += CLIM_LINE_H;
    }

    /* Pressure row */
    if(has_press) {
        snprintf(buf, sizeof(buf), "%.0f %s", (double)clim_sensor->pressure, press_unit_str);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, y + CLIM_LINE_H, AlignCenter, AlignBottom, buf);
    }

    /* Debug overlay: last raw ppm (top of screen, only in debug mode) */
    if(co2_sensor && app->settings.debug_mode) {
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            buf,
            sizeof(buf),
            "raw:%ld %ld/%ld",
            (long)co2_sensor->dbg_ppm_raw,
            (long)co2_sensor->dbg_th,
            (long)co2_sensor->dbg_tl);
        canvas_draw_str_aligned(canvas, 0, 7, AlignLeft, AlignBottom, buf);
    }

    /* Clock + battery (top corners, if enabled) */
    if(app->settings.show_status) {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour, dt.minute);
        canvas_draw_str_aligned(canvas, 0, 7, AlignLeft, AlignBottom, buf);

        uint8_t pct = furi_hal_power_get_pct();
        snprintf(buf, sizeof(buf), "%d%%", pct);
        canvas_draw_str_aligned(canvas, SCREEN_W, 7, AlignRight, AlignBottom, buf);
    }
}

/* ---- Input callback ---- */

static bool input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    air_stats_backlight_activity();
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        view_main_menu_switch();
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        if(is_co2_frozen()) {
            Sensor* s = find_co2_sensor();
            if(s) s->needs_reset = true;
            return true;
        }
    }
    return false;
}

/* ---- Back = exit app ---- */

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/* ---- Public API ---- */

void view_main_alloc(void) {
    view = view_alloc();
    view_set_draw_callback(view, draw_callback);
    view_set_input_callback(view, input_callback);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_main_switch(void) {
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_main_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    view_free(view);
}
