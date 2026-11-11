#include "gpio_input_monitor.h"
#include "../gpio_items.h"

#include <gui/elements.h>
#include <stdio.h>

struct GpioInputMonitor {
    View* view;
};

typedef struct {
    GPIOItems* gpio_items;
} GpioInputMonitorModel;

static void gpio_input_monitor_draw_callback(Canvas* canvas, void* _model) {
    GpioInputMonitorModel* model = _model;
    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, "GPIO Logic Monitor");
    canvas_set_font(canvas, FontSecondary);

    const uint8_t count = gpio_items_get_count(model->gpio_items);
    char text[16];
    for(uint8_t i = 0; i < count; i++) {
        const int32_t x = (i % 2) ? 68 : 4;
        const int32_t y = 24 + (int32_t)(i / 2) * 11;
        if(y > 62) break;
        snprintf(
            text,
            sizeof(text),
            "%s: %c",
            gpio_items_get_pin_name(model->gpio_items, i),
            gpio_items_read_pin(model->gpio_items, i) ? 'H' : 'L');
        canvas_draw_str(canvas, x, y, text);
    }
}

GpioInputMonitor* gpio_input_monitor_alloc(GPIOItems* gpio_items) {
    GpioInputMonitor* gpio_input_monitor = malloc(sizeof(GpioInputMonitor));

    gpio_input_monitor->view = view_alloc();
    view_allocate_model(
        gpio_input_monitor->view, ViewModelTypeLocking, sizeof(GpioInputMonitorModel));

    with_view_model(
        gpio_input_monitor->view,
        GpioInputMonitorModel * model,
        { model->gpio_items = gpio_items; },
        false);

    view_set_context(gpio_input_monitor->view, gpio_input_monitor);
    view_set_draw_callback(gpio_input_monitor->view, gpio_input_monitor_draw_callback);

    return gpio_input_monitor;
}

void gpio_input_monitor_free(GpioInputMonitor* gpio_input_monitor) {
    furi_assert(gpio_input_monitor);
    view_free(gpio_input_monitor->view);
    free(gpio_input_monitor);
}

View* gpio_input_monitor_get_view(GpioInputMonitor* gpio_input_monitor) {
    furi_assert(gpio_input_monitor);
    return gpio_input_monitor->view;
}

void gpio_input_monitor_update(GpioInputMonitor* gpio_input_monitor) {
    furi_assert(gpio_input_monitor);
    with_view_model(
        gpio_input_monitor->view, GpioInputMonitorModel * model, { UNUSED(model); }, true);
}
