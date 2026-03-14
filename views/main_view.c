/*
 * main_view.c — main sensor display screen.
 * Shows CO2 ppm (MH-Z19C) + temperature/humidity/pressure (BME280).
 * OK button → main menu.  Back → exit app.
 */
#include "../co2_app_i.h"
#include <string.h>

static View* view;

#define VIEW_ID ViewMain

/* ---- Helpers to find sensors by typename ---- */

static Sensor* find_sensor(const char* typename) {
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(app->sensors[i] && strcmp(app->sensors[i]->type->typename, typename) == 0) {
            return app->sensors[i];
        }
    }
    return NULL;
}

/* ---- Draw callback ---- */

static void draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_clear(canvas);
    char buf[48];

    Sensor* co2_sensor = find_sensor("MHZ19C");
    Sensor* bme_sensor = find_sensor("BME280");

    /* CO2 — large font.
     * MH-Z19C works via PWM edge detection: status flips between OK (edge caught)
     * and POLLING (waiting for next edge, ~900ms/cycle). We must NOT gate on
     * status == OK or the display blinks. Instead check co2 > 0 which preserves
     * the last valid reading between edge detections. */
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

    /* BME280 — small font */
    canvas_set_font(canvas, FontSecondary);
    if(bme_sensor && bme_sensor->status == UT_SENSORSTATUS_OK) {
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
        snprintf(
            buf,
            sizeof(buf),
            "T:%.1f%s H:%.0f%% P:%.0f%s",
            (double)bme_sensor->temp,
            temp_unit_str,
            (double)bme_sensor->hum,
            (double)bme_sensor->pressure,
            press_unit_str);
        canvas_draw_str(canvas, 0, 36, buf);
    } else {
        canvas_draw_str(canvas, 0, 36, "BME280: not found");
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
