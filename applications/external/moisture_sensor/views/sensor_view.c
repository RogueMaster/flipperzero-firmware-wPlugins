#include "sensor_view.h"
#include <gui/elements.h>
#include <furi.h>

struct SensorView {
    View* view;
    SensorViewMenuCallback menu_callback;
    void* menu_context;
};

typedef struct {
    uint16_t raw_adc;
    uint16_t millivolts;
    uint8_t moisture_percent;
    uint8_t channel;
    bool sensor_connected;
} SensorViewModel;

static const char* get_moisture_status(uint8_t percent) {
    if(percent < 20) return "Very Dry";
    if(percent < 40) return "Dry";
    if(percent < 60) return "Moist";
    if(percent < 80) return "Wet";
    return "Very Wet";
}

static void draw_sensor_connected(Canvas* canvas, SensorViewModel* m, const char* info_str) {
    char num_str[4];
    snprintf(num_str, sizeof(num_str), "%d", m->moisture_percent);

    canvas_set_font(canvas, FontBigNumbers);
    uint16_t num_width = canvas_string_width(canvas, num_str);
    canvas_set_font(canvas, FontPrimary);
    uint16_t pct_width = canvas_string_width(canvas, "%");
    uint16_t total_width = num_width + 2 + pct_width;
    uint16_t start_x = (128 - total_width) / 2;

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, start_x, 34, num_str);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, start_x + num_width + 2, 34, "%");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 36, AlignCenter, AlignTop, get_moisture_status(m->moisture_percent));
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignTop, info_str);

    canvas_draw_frame(canvas, 14, 58, 100, 5);
    if(m->moisture_percent > 0) {
        canvas_draw_box(canvas, 14, 58, m->moisture_percent, 5);
    }
}

static void draw_sensor_disconnected(Canvas* canvas, const char* info_str) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "No Sensor");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "Connect to pin 16");
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, info_str);
}

static void sensor_view_draw_callback(Canvas* canvas, void* model) {
    SensorViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 10, "< cfg");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 126, 2, AlignRight, AlignTop, "Moisture Sensor");
    canvas_draw_line(canvas, 0, 14, 128, 14);

    char info_str[32];
    snprintf(
        info_str, sizeof(info_str), "%dmV  ADC:%d  Ch:%d", m->millivolts, m->raw_adc, m->channel);

    if(m->sensor_connected) {
        draw_sensor_connected(canvas, m, info_str);
    } else {
        draw_sensor_disconnected(canvas, info_str);
    }
}

static bool sensor_view_input_callback(InputEvent* event, void* context) {
    SensorView* sensor_view = context;

    if(event->type == InputTypePress && event->key == InputKeyLeft) {
        if(sensor_view->menu_callback) {
            sensor_view->menu_callback(sensor_view->menu_context);
        }
        return true;
    }
    return false;
}

SensorView* sensor_view_alloc(void) {
    SensorView* sensor_view = malloc(sizeof(SensorView));
    if(!sensor_view) return NULL;

    sensor_view->view = view_alloc();
    if(!sensor_view->view) {
        free(sensor_view);
        return NULL;
    }

    sensor_view->menu_callback = NULL;
    sensor_view->menu_context = NULL;

    view_set_context(sensor_view->view, sensor_view);
    view_allocate_model(sensor_view->view, ViewModelTypeLocking, sizeof(SensorViewModel));
    view_set_draw_callback(sensor_view->view, sensor_view_draw_callback);
    view_set_input_callback(sensor_view->view, sensor_view_input_callback);

    return sensor_view;
}

void sensor_view_free(SensorView* sensor_view) {
    view_free(sensor_view->view);
    free(sensor_view);
}

View* sensor_view_get_view(SensorView* sensor_view) {
    return sensor_view->view;
}

void sensor_view_set_menu_callback(
    SensorView* sensor_view,
    SensorViewMenuCallback callback,
    void* context) {
    sensor_view->menu_callback = callback;
    sensor_view->menu_context = context;
}

void sensor_view_update(
    SensorView* sensor_view,
    uint16_t raw_adc,
    uint16_t millivolts,
    uint8_t channel,
    uint8_t moisture_percent,
    bool connected) {
    with_view_model(
        sensor_view->view,
        SensorViewModel * model,
        {
            model->raw_adc = raw_adc;
            model->millivolts = millivolts;
            model->channel = channel;
            model->moisture_percent = moisture_percent;
            model->sensor_connected = connected;
        },
        true);
}
