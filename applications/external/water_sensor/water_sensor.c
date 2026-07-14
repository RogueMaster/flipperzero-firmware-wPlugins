#include "water_sensor.h"
// Made by Matvey Strelov
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdio.h>

typedef struct {
    ViewPort* view_port;
    Gui* gui;
    FuriHalAdcHandle* adc;
    uint16_t raw;
    float mv;
    bool running;
} WaterSensorApp;

static void draw_callback(Canvas* canvas, void* ctx) {
    WaterSensorApp* app = ctx;
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    char line[64];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Water Sensor reader");

    snprintf(line, sizeof(line), "RAW: %u", app->raw);
    canvas_draw_str_aligned(canvas, 8, 24, AlignLeft, AlignTop, line);

    snprintf(line, sizeof(line), "V: %.1f mV", (double)app->mv);
    canvas_draw_str_aligned(canvas, 8, 36, AlignLeft, AlignTop, line);

    const float max_mv = 2000.0f;
    float progress = app->mv / max_mv;
    if(progress < 0.0f) progress = 0.0f;
    if(progress > 1.0f) progress = 1.0f;

    char pct_text[16];
    snprintf(pct_text, sizeof(pct_text), "%0.0f%%", (double)(progress * 100.0f));

    elements_progress_bar_with_text(canvas, 8, 48, 112, progress, pct_text);
}

static void input_callback(InputEvent* event, void* ctx) {
    WaterSensorApp* app = ctx;
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->running = false;
    }
}

int32_t water_sensor_main(void* p) {
    UNUSED(p);

    WaterSensorApp app;
    app.view_port = view_port_alloc();
    app.gui = furi_record_open("gui");
    app.raw = 0;
    app.mv = 0.0f;
    app.running = true;

    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, &app);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    furi_hal_gpio_init(ADC_GPIO, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);

    app.adc = furi_hal_adc_acquire();
    furi_hal_adc_configure(app.adc);

    while(app.running) {
        uint16_t raw = furi_hal_adc_read(app.adc, ADC_CHANNEL);
        float mv = furi_hal_adc_convert_to_voltage(app.adc, raw);

        app.raw = raw;
        app.mv = mv;

        view_port_update(app.view_port);
        furi_delay_ms(UPDATE_MS);
    }

    furi_hal_adc_release(app.adc);
    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_record_close("gui");

    return 0;
}
