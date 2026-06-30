/* 
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "current_gauge.h"
#include "utils.h"

#include <gui/elements.h>

typedef enum {
    CurrentGaugeRange_A,
    CurrentGaugeRange_mA,
} CurrentGaugeRange;

struct CurrentGauge {
    View* view;
    CurrentGaugeButtonCallback button_callback;
    void* callback_context;
};

typedef struct {
    CurrentGaugeRange range;
    SensorState sensor_state;
} GaugeModel;

static void current_gauge_draw_callback(Canvas* canvas, void* _model) {
    GaugeModel* model = (GaugeModel*)_model;
    furi_check(model);

    FuriString* value_text = furi_string_alloc();
    const char* unit_text = "";

    // Top bar
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);

    // Sensor type and shunt resistor value
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 10, model->sensor_state.sensor_type);

    FuriString* shunt_text = format_resistance_value(model->sensor_state.shunt_resistor);
    canvas_draw_str_aligned(
        canvas, 126, 10, AlignRight, AlignBottom, furi_string_get_cstr(shunt_text));
    furi_string_free(shunt_text);

    // Left and right borders
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 0, 0, 64);
    canvas_draw_line(canvas, 127, 0, 127, 64);

    // Ticks
    canvas_set_color(canvas, ColorBlack);

    for(int tick = -6; tick <= 2; tick++) {
        int32_t x = 8 + 14 * (tick + 6);
        canvas_draw_line(canvas, x, 13, x, 14);
    }

    // Current value (vertical line)
    double log_of_current = MAX(-6.0, MIN(2.0, log10f(fabs(model->sensor_state.current))));
    int32_t x = round(8 + 14 * (log_of_current + 6));
    canvas_draw_line(canvas, x, 13, x, 19);

    if(model->sensor_state.ready) {
        // Current value
        if(model->range == CurrentGaugeRange_mA) {
            furi_string_printf(value_text, "%5.3f", (double)1000.0 * model->sensor_state.current);
            unit_text = "mA";
        } else {
            furi_string_printf(value_text, "%5.3f", model->sensor_state.current);
            unit_text = "A";
        }
    } else {
        furi_string_printf(value_text, "-.---");
        unit_text = "";
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas, 94, 40, AlignRight, AlignBottom, furi_string_get_cstr(value_text));
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 96, 40, unit_text);

    if(model->sensor_state.ready) {
        // Bus voltage value
        furi_string_printf(value_text, "%6.3f V", model->sensor_state.voltage);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 53, 61, furi_string_get_cstr(value_text));
    }

    // Buttons
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "Config");
    elements_button_right(canvas, "Log");

    furi_string_free(value_text);
}

static void invoke_button_callback(CurrentGauge* gauge, CurrentGaugeButton button) {
    if(gauge->button_callback) {
        gauge->button_callback(gauge->callback_context, button);
    }
}

bool current_gauge_input_callback(InputEvent* event, void* context) {
    CurrentGauge* gauge = (CurrentGauge*)context;
    furi_check(gauge != NULL);

    if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        invoke_button_callback(gauge, CurrentGaugeButton_Menu);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        invoke_button_callback(gauge, CurrentGaugeButton_DataLog);
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyDown) {
        invoke_button_callback(gauge, CurrentGaugeButton_ShuntSwitch);
        return true;
    }

    return false;
}

CurrentGauge* current_gauge_alloc(void) {
    CurrentGauge* gauge = (CurrentGauge*)malloc(sizeof(CurrentGauge));

    gauge->view = view_alloc();
    view_set_context(gauge->view, gauge);
    view_allocate_model(gauge->view, ViewModelTypeLocking, sizeof(GaugeModel));
    view_set_draw_callback(gauge->view, current_gauge_draw_callback);
    view_set_input_callback(gauge->view, current_gauge_input_callback);

    return gauge;
}

View* current_gauge_get_view(CurrentGauge* gauge) {
    furi_check(gauge != NULL);

    return gauge->view;
}

void current_gauge_free(CurrentGauge* gauge) {
    furi_check(gauge != NULL);

    view_free_model(gauge->view);
    view_free(gauge->view);
    free(gauge);
}

void current_gauge_update(CurrentGauge* gauge, const SensorState* state) {
    with_view_model(
        gauge->view,
        GaugeModel * model,
        {
            model->sensor_state = *state;

            if(fabs(model->sensor_state.current) < (double)0.1) {
                model->range = CurrentGaugeRange_mA;
            } else {
                model->range = CurrentGaugeRange_A;
            }
        },
        true);
}

void current_gauge_set_button_callback(
    CurrentGauge* gauge,
    CurrentGaugeButtonCallback callback,
    void* context) {
    furi_check(gauge != NULL);

    gauge->button_callback = callback;
    gauge->callback_context = context;
}
