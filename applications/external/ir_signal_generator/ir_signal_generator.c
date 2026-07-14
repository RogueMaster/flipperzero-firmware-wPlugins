#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_infrared.h>
#include <furi_hal_power.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <input/input.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    bool is_generating;
    uint32_t frequency;
    uint8_t anim_phase;
    bool show_splash;
    bool show_rx_screen;
    int32_t rx_frequency;
    FuriMutex* mutex;
    uint32_t splash_start_time;
    bool using_external_tx;
    bool is_rx_active;
} GeneratorState;

GeneratorState app_state = {
    .is_generating = false,
    .frequency = 38000,
    .anim_phase = 0,
    .show_splash = true,
    .show_rx_screen = false,
    .rx_frequency = -1,
    .splash_start_time = 0,
    .using_external_tx = false,
    .is_rx_active = false,
};

static FuriHalInfraredTxGetDataState
    generator_tx_data_callback(void* context, uint32_t* duration, bool* level) {
    UNUSED(context);
    *duration = 100000;
    *level = true;
    return FuriHalInfraredTxGetDataStateLastDone;
}

static void draw_rounded_box(Canvas* canvas, int x, int y, int w, int h, const char* text) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, x, y, w, h, 3);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2 + 1, AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
}

static void ir_received_callback(void* context, InfraredWorkerSignal* signal) {
    UNUSED(context);
    furi_check(signal);
    furi_mutex_acquire(app_state.mutex, FuriWaitForever);

    if(infrared_worker_signal_is_decoded(signal)) {
        const InfraredMessage* message = infrared_worker_get_decoded_signal(signal);
        app_state.rx_frequency = infrared_get_protocol_frequency(message->protocol);

        if(app_state.rx_frequency > 0) {
            app_state.frequency = app_state.rx_frequency;
        }
    } else {
        app_state.rx_frequency = 0;
    }

    furi_mutex_release(app_state.mutex);
}

static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    furi_mutex_acquire(app_state.mutex, FuriWaitForever);
    canvas_clear(canvas);

    if(app_state.show_splash) {
        uint32_t tick = furi_get_tick();
        int step = (tick / 150) % 6;

        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, "by");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Sacriphanius");

        int center_x_left = 15;
        int center_x_right = 113;
        int y = 40;

        for(int i = 0; i < 3; i++) {
            int wave_age = (step - i);
            if(wave_age >= 0 && wave_age < 3) {
                int offset = wave_age * 6;
                canvas_draw_str_aligned(
                    canvas, center_x_left - offset, y, AlignCenter, AlignCenter, "(");
                canvas_draw_str_aligned(
                    canvas, center_x_right + offset, y, AlignCenter, AlignCenter, ")");
            }
        }

        furi_mutex_release(app_state.mutex);
        return;
    } else if(app_state.show_rx_screen) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 14);

        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Signal Searcher");

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, 2, 16, 124, 38, 10);

        if(app_state.is_rx_active) {
            int bar_w = 6;
            int gap = 4;
            int num_bars = 10;
            int start_x = 16;
            int bottom_y = 48;

            for(int i = 0; i < num_bars; i++) {
                int x = start_x + i * (bar_w + gap);
                int h = 0;

                if(app_state.rx_frequency > 0) {
                    float t = (float)app_state.anim_phase * 0.4f;
                    h = 14 + (int)(10.0f * sinf(t + i * 0.8f)) + (rand() % 6);
                } else {
                    float t = (float)app_state.anim_phase * 0.15f;
                    h = 12 + (int)(8.0f * sinf(t + i * 0.5f));
                }

                if(h > 30) h = 30;
                if(h < 3) h = 3;

                canvas_draw_box(canvas, x, bottom_y - h, bar_w, h);
            }
        }

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 55, 128, 9);

        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);

        int text_y = 60;

        if(!app_state.is_rx_active) {
            canvas_draw_str_aligned(
                canvas, 64, text_y, AlignCenter, AlignCenter, "Press OK to Start");
        } else {
            if(app_state.rx_frequency > 0) {
                char freq_buf[20];
                snprintf(
                    freq_buf, sizeof(freq_buf), "Found: %lu kHz", app_state.rx_frequency / 1000);

                canvas_set_font(canvas, FontPrimary);
                canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, freq_buf);
            } else if(app_state.rx_frequency == -1) {
                canvas_draw_str_aligned(
                    canvas, 64, text_y, AlignCenter, AlignCenter, "Scanning...");
            } else {
                canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, "Use 38khz");
            }
        }
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, 0, -5, 128, 17, 5);

        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "IR Signal Generator");
        canvas_set_color(canvas, ColorBlack);

        canvas_draw_line(canvas, 36, 14, 36, 54);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 18, 20, AlignCenter, AlignCenter, "FREQ");

        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%lu", app_state.frequency / 1000);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 18, 35, AlignCenter, AlignCenter, buffer);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 18, 45, AlignCenter, AlignCenter, "kHz");

        const int frame_x = 40;
        const int frame_y = 15;
        const int frame_w = 84;
        const int frame_h = 37;
        const int bottom_y = frame_y + frame_h - 1;

        canvas_draw_rframe(canvas, frame_x, frame_y, frame_w, frame_h, 4);

        canvas_set_font(canvas, FontSecondary);
        if(app_state.is_generating) {
            if(app_state.using_external_tx) {
                canvas_draw_str(canvas, frame_x + 3, frame_y + 8, "ext");
            } else {
                canvas_draw_str(canvas, frame_x + 3, frame_y + 8, "int");
            }
        } else {
            canvas_draw_str(canvas, frame_x + 3, frame_y + 8, "int");
        }

        int tx_bar_w = 5;
        int tx_gap = 3;
        int tx_num_bars = 10;
        int tx_start_x = frame_x + 2;

        for(int i = 0; i < tx_num_bars; i++) {
            int x = tx_start_x + i * (tx_bar_w + tx_gap);
            int h = 0;

            if(app_state.is_generating) {
                float t = (float)app_state.anim_phase * 0.5f;
                h = 10 + (int)(8.0f * sinf(t + i * 0.9f)) + (rand() % 8);
                if(h > 24) h = 24;
            } else {
                float t = (float)app_state.anim_phase * 0.1f;
                h = 8 + (int)(5.0f * sinf(t + i * 0.4f));
            }

            if(h < 2) h = 2;

            canvas_draw_box(canvas, x, bottom_y - h, tx_bar_w, h);
        }

        int btn_y = 55;
        int btn_h = 9;
        draw_rounded_box(canvas, 0, btn_y, 24, btn_h, "< F >");

        if(app_state.is_generating) {
            draw_rounded_box(canvas, 52, btn_y, 24, btn_h, "STOP");
        } else {
            draw_rounded_box(canvas, 52, btn_y, 24, btn_h, "TX");
        }

        draw_rounded_box(canvas, 104, btn_y, 24, btn_h, "");
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 116, 59, AlignCenter, AlignCenter, "RX ^");
        canvas_set_color(canvas, ColorBlack);
    }

    furi_mutex_release(app_state.mutex);
}

static void update_led_effect() {
    uint32_t freq = app_state.frequency;
    bool led_on = (app_state.anim_phase % 4) < 2;

    if(!led_on) {
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 0);
        furi_hal_light_set(LightBlue, 0);
        return;
    }

    uint8_t r = 0, g = 0, b = 0;
    if(freq < 35000) {
        r = 255;
    } else if(freq < 42000) {
        g = 255;
    } else if(freq < 50000) {
        b = 255;
    } else {
        r = 150;
        b = 255;
    }
    furi_hal_light_set(LightRed, r);
    furi_hal_light_set(LightGreen, g);
    furi_hal_light_set(LightBlue, b);
}

static void start_generating() {
    FuriHalInfraredTxPin output_pin = furi_hal_infrared_detect_tx_output();

    if(output_pin == FuriHalInfraredTxPinExtPA7) {
        furi_hal_power_enable_otg();
        app_state.using_external_tx = true;
    } else {
        app_state.using_external_tx = false;
    }

    furi_hal_infrared_set_tx_output(output_pin);

    app_state.is_generating = true;
}

static void stop_generating() {
    if(furi_hal_infrared_is_busy()) {
        furi_hal_infrared_async_tx_stop();
    }

    app_state.is_generating = false;

    if(app_state.using_external_tx) {
        furi_hal_power_disable_otg();
    }

    furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);

    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t ir_signal_generator_app(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, &app_state);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;
    app_state.splash_start_time = furi_get_tick();
    app_state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    InfraredWorker* worker = infrared_worker_alloc();

    while(running) {
        if(app_state.show_splash) {
            if(furi_get_tick() - app_state.splash_start_time > 2000) {
                app_state.show_splash = false;
            }
            view_port_update(view_port);
            if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
                if(event.key == InputKeyBack) running = false;
            }
            continue;
        } else if(app_state.show_rx_screen) {
            if(app_state.is_rx_active) {
                app_state.anim_phase++;
            }

            if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
                if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                    switch(event.key) {
                    case InputKeyBack:
                        if(app_state.is_rx_active) {
                            infrared_worker_rx_stop(worker);
                            app_state.is_rx_active = false;
                        } else {
                            running = false;
                        }
                        break;
                    case InputKeyOk:
                        if(!app_state.is_rx_active) {
                            infrared_worker_rx_enable_signal_decoding(worker, true);
                            infrared_worker_rx_enable_blink_on_receiving(worker, true);
                            infrared_worker_rx_set_received_signal_callback(
                                worker, ir_received_callback, &app_state);
                            infrared_worker_rx_start(worker);
                            app_state.is_rx_active = true;
                            app_state.rx_frequency = -1;
                        }
                        break;
                    case InputKeyUp:
                        if(app_state.is_rx_active) {
                            infrared_worker_rx_stop(worker);
                            app_state.is_rx_active = false;
                        }
                        app_state.show_rx_screen = false;
                        break;
                    default:
                        break;
                    }
                }
            }
            view_port_update(view_port);
        } else if(app_state.is_generating) {
            app_state.anim_phase++;
            update_led_effect();
            furi_hal_infrared_async_tx_set_data_isr_callback(generator_tx_data_callback, NULL);
            furi_hal_infrared_async_tx_start(app_state.frequency, 0.50f);
            furi_delay_ms(50);
            furi_hal_infrared_async_tx_stop();

            if(furi_message_queue_get(event_queue, &event, 0) == FuriStatusOk) {
                if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                    if(event.key == InputKeyBack) {
                        running = false;
                        stop_generating();
                    } else if(event.key == InputKeyOk) {
                        stop_generating();
                    } else if(event.key == InputKeyRight) {
                        if(app_state.frequency <= 60000) app_state.frequency += 1000;
                    } else if(event.key == InputKeyLeft) {
                        if(app_state.frequency >= 30000) app_state.frequency -= 1000;
                    }
                }
            }
            view_port_update(view_port);
        } else {
            app_state.anim_phase++;
            furi_hal_light_set(LightRed, 0);
            furi_hal_light_set(LightGreen, 0);
            furi_hal_light_set(LightBlue, 0);

            if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
                if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                    switch(event.key) {
                    case InputKeyBack:
                        running = false;
                        break;
                    case InputKeyOk:
                        start_generating();
                        break;
                    case InputKeyRight:
                        if(app_state.frequency <= 60000) app_state.frequency += 1000;
                        break;
                    case InputKeyLeft:
                        if(app_state.frequency >= 30000) app_state.frequency -= 1000;
                        break;
                    default:
                        break;
                    }
                } else if(event.type == InputTypeLong) {
                    switch(event.key) {
                    case InputKeyUp:
                        app_state.show_rx_screen = true;
                        app_state.is_rx_active = false;
                        app_state.rx_frequency = -1;
                        break;
                    default:
                        break;
                    }
                }
            }
            view_port_update(view_port);
        }
    }

    if(app_state.is_generating) {
        stop_generating();
    }
    if(app_state.is_rx_active) {
        infrared_worker_rx_stop(worker);
    }
    infrared_worker_free(worker);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(app_state.mutex);
    furi_record_close(RECORD_GUI);

    return 0;
}
