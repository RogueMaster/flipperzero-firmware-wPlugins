/*
 * main_view.c — main sensor display screen.
 * Shows CO2 ppm (any CO2 sensor) + readings from all other active sensors.
 * OK button → main menu.  Back → exit app.
 */
#include "../air_stats_i.h"

static View* view;

#define VIEW_ID ViewMain

/* ---- Draw callback ---- */

static void draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_clear(canvas);
    if(!app->sensors_ready || !app->sensors) return;
    char buf[48];

    /* Find first CO2 sensor */
    Sensor* co2_sensor = NULL;
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        Sensor* s = app->sensors[i];
        if(s && s->status != UT_SENSORSTATUS_INACTIVE &&
           (s->type->datatype & UT_CO2)) {
            co2_sensor = s;
            break;
        }
    }

    /* CO2 — large font.
     * MH-Z19C uses PWM edge detection: co2 > 0 preserves last valid reading
     * between edges so the display doesn't blink. */
    bool co2_valid = co2_sensor && co2_sensor->co2 > 0.0f;
    canvas_set_font(canvas, FontPrimary);
    if(co2_valid) {
        snprintf(buf, sizeof(buf), "CO2: %d ppm", (int)co2_sensor->co2);
    } else {
        snprintf(buf, sizeof(buf), "CO2: --");
    }
    canvas_draw_str(canvas, 0, 12, buf);

    /* CO2 bar graph (0..2000 ppm) */
    canvas_draw_frame(canvas, 0, 15, 128, 7);
    if(co2_valid) {
        int32_t clamped = (int32_t)co2_sensor->co2 > 2000 ? 2000 : (int32_t)co2_sensor->co2;
        uint8_t fill = (uint8_t)(clamped * 126 / 2000);
        if(fill > 0) canvas_draw_box(canvas, 1, 16, fill, 5);
    }

    /* All other active sensors — small font, one line each */
    canvas_set_font(canvas, FontSecondary);
    uint8_t y = 34;

    const char* temp_unit_str = (app->settings.temp_unit == UT_TEMP_FAHRENHEIT) ? "F" : "C";
    const char* press_unit_str;
    switch(app->settings.pressure_unit) {
    case UT_PRESSURE_IN_HG: press_unit_str = "inHg"; break;
    case UT_PRESSURE_KPA:   press_unit_str = "kPa";  break;
    case UT_PRESSURE_HPA:   press_unit_str = "hPa";  break;
    default:                press_unit_str = "mmHg"; break;
    }

    /* In UART CO2 mode climate sensors are disabled — show notice */
    if(app->settings.co2_type == CO2_TYPE_UART) {
        canvas_draw_str(canvas, 0, y, "Climate: N/A (UART mode)");
        y += 11;
    }

    for(uint8_t i = 0; i < app->sensors_count && y <= 54; i++) {
        Sensor* s = app->sensors[i];
        if(!s || s->status == UT_SENSORSTATUS_INACTIVE || s == co2_sensor) continue;

        if(s->status == UT_SENSORSTATUS_OK) {
            int pos = 0;
            if(s->type->datatype & UT_TEMPERATURE) {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "T:%.1f%s", (double)s->temp, temp_unit_str);
            }
            if(s->type->datatype & UT_HUMIDITY) {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                    " H:%.0f%%", (double)s->hum);
            }
            if(s->type->datatype & UT_PRESSURE) {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                    " P:%.0f%s", (double)s->pressure, press_unit_str);
            }
            if((s->type->datatype & UT_CO2) && s->co2 > 0.0f) {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                    " CO2:%d", (int)s->co2);
            }
            if(pos == 0) buf[0] = '\0';
        } else {
            snprintf(buf, sizeof(buf), "%s: --", s->name);
        }

        if(buf[0]) {
            canvas_draw_str(canvas, 0, y, buf);
            y += 11;
        }
    }

    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, "[OK=menu]");
}

/* ---- Input callback ---- */

static bool input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        view_main_menu_switch();
        return true;
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
