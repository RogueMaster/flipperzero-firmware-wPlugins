#include "co2_app_i.h"

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    AppData d = app->data;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    char buf[48];

    // CO2 — large font
    canvas_set_font(canvas, FontPrimary);
    if(d.co2_connected) {
        snprintf(buf, sizeof(buf), "CO2: %d ppm", (int)d.co2_ppm);
    } else {
        snprintf(buf, sizeof(buf), "CO2: --");
    }
    canvas_draw_str(canvas, 0, 12, buf);

    // CO2 bar graph (0..2000 ppm)
    canvas_draw_frame(canvas, 0, 15, 128, 7);
    if(d.co2_connected) {
        int32_t clamped = d.co2_ppm > 2000 ? 2000 : d.co2_ppm;
        uint8_t fill = (uint8_t)(clamped * 126 / 2000);
        if(fill > 0) canvas_draw_box(canvas, 1, 16, fill, 5);
    }

    // BME280 — small font
    canvas_set_font(canvas, FontSecondary);
    if(d.bme280_valid) {
        snprintf(
            buf,
            sizeof(buf),
            "T:%.1fC H:%.0f%% P:%.0fhPa",
            (double)d.temperature,
            (double)d.humidity,
            (double)d.pressure);
        canvas_draw_str(canvas, 0, 36, buf);
    } else {
        canvas_draw_str(canvas, 0, 36, "BME280: not found");
    }

    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, "[back]");
}

static void input_callback(InputEvent* event, void* ctx) {
    furi_message_queue_put((FuriMessageQueue*)ctx, event, 0);
}

int32_t co2_app_main(void* p) {
    UNUSED(p);

    App app = {0};
    app.gui         = furi_record_open(RECORD_GUI);
    app.event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.mutex       = furi_mutex_alloc(FuriMutexTypeNormal);
    app.view_port   = view_port_alloc();

    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, app.event_queue);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    // MH-Z19 PWM init (verbatim from flipper-zero-mh-z19)
    app.co2_pin = &gpio_ext_pa6;
    furi_hal_gpio_init(app.co2_pin, GpioModeInput, GpioPullUp, GpioSpeedVeryHigh);
    app.co2_range = RANGE_2000;

    furi_delay_ms(50); // let power rail stabilize before BME280 probe
    bme280_init(&app);

    InputEvent event;
    uint32_t bme_tick = 0;

    while(true) {
        // Poll GPIO and calculate CO2 PPM (100ms interval, verbatim algorithm)
        bool quit = furi_message_queue_get(app.event_queue, &event, 100) == FuriStatusOk;
        if(quit) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) break;
        }

        int32_t gpio_val = furi_hal_gpio_read(app.co2_pin) ? 1 : 0;
        int32_t ppm = calculate_ppm(
            &app.co2_prevVal,
            gpio_val,
            &app.co2_th,
            &app.co2_tl,
            &app.co2_h,
            &app.co2_l,
            app.co2_range);

        // Read BME280 every ~5 seconds (50 × 100ms)
        bme_tick++;
        float temp = 0, hum = 0, press = 0;
        bool bme_ok = false;
        if(bme_tick >= 50) {
            bme_tick = 0;
            if(!app.bme280_found) bme280_init(&app);
            bme_ok = bme280_read(&app, &temp, &hum, &press);
        }

        // Update shared data atomically
        furi_mutex_acquire(app.mutex, FuriWaitForever);
        if(ppm > 0) {
            app.data.co2_ppm       = ppm;
            app.data.co2_connected = true;
        }
        if(bme_tick == 0) {
            app.data.bme280_valid = bme_ok;
            if(bme_ok) {
                app.data.temperature = temp;
                app.data.humidity    = hum;
                app.data.pressure    = press;
            }
        }
        furi_mutex_release(app.mutex);

        view_port_update(app.view_port);
    }

    bme280_deinit(&app);

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_mutex_free(app.mutex);
    furi_message_queue_free(app.event_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}
