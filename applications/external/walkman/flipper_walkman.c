#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

typedef enum {
    EventTypeInput,
    EventTypeTick,
} WalkmanEventType;

typedef enum {
    ScreenSplash,
    ScreenPlayer,
    ScreenAbout,
    ScreenNoModule,
} WalkmanScreen;

typedef struct {
    WalkmanEventType type;
    InputEvent input;
} WalkmanEvent;

typedef struct {
    FuriMessageQueue* queue;
    ViewPort* view_port;
    FuriTimer* timer;
    FuriHalSerialHandle* serial;

    WalkmanScreen screen;
    bool playing;
    bool module_attached;
    volatile uint8_t probe_rx_count;
    uint8_t volume;
    uint8_t reel_frame;
    uint8_t progress;
    uint8_t splash_ticks;
} WalkmanApp;

static void redraw(WalkmanApp* app) {
    view_port_update(app->view_port);
}

static void uart_send(FuriHalSerialHandle* serial, const uint8_t* data, size_t len) {
    furi_hal_serial_tx(serial, data, len);
    furi_delay_ms(100);
}

static void cmd_set_volume(FuriHalSerialHandle* serial, uint8_t volume) {
    if(volume > 30) volume = 30;
    const uint8_t cmd[] = {0x7E, 0xFF, 0x06, 0x06, 0x00, 0x00, volume, 0xEF};
    uart_send(serial, cmd, sizeof(cmd));
}

static void cmd_play_first(FuriHalSerialHandle* serial) {
    const uint8_t cmd[] = {0x7E, 0xFF, 0x06, 0x0F, 0x00, 0x01, 0x01, 0xEF};
    uart_send(serial, cmd, sizeof(cmd));
}

static void cmd_pause(FuriHalSerialHandle* serial) {
    const uint8_t cmd[] = {0x7E, 0xFF, 0x06, 0x0E, 0x00, 0x00, 0x00, 0xEF};
    uart_send(serial, cmd, sizeof(cmd));
}

static void cmd_resume(FuriHalSerialHandle* serial) {
    const uint8_t cmd[] = {0x7E, 0xFF, 0x06, 0x0D, 0x00, 0x00, 0x00, 0xEF};
    uart_send(serial, cmd, sizeof(cmd));
}

static void cmd_next(FuriHalSerialHandle* serial) {
    const uint8_t cmd[] = {0x7E, 0xFF, 0x06, 0x01, 0x00, 0x00, 0x00, 0xEF};
    uart_send(serial, cmd, sizeof(cmd));
}

static void cmd_prev(FuriHalSerialHandle* serial) {
    const uint8_t cmd[] = {0x7E, 0xFF, 0x06, 0x02, 0x00, 0x00, 0x00, 0xEF};
    uart_send(serial, cmd, sizeof(cmd));
}

static uint16_t yx_checksum(uint8_t cmd, uint8_t feedback, uint8_t param_hi, uint8_t param_lo) {
    uint16_t sum = 0xFF + 0x06 + cmd + feedback + param_hi + param_lo;
    return (uint16_t)(0 - sum);
}

static void serial_probe_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    WalkmanApp* app = context;

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            (void)furi_hal_serial_async_rx(handle);
            if(app->probe_rx_count < UINT8_MAX) {
                app->probe_rx_count++;
            }
        }
    }
}

static bool detect_mp3_module(WalkmanApp* app) {
    app->probe_rx_count = 0;

    furi_hal_serial_async_rx_start(app->serial, serial_probe_rx_callback, app, false);

    const uint8_t cmd = 0x42;
    const uint8_t feedback = 0x01;
    const uint8_t param_hi = 0x00;
    const uint8_t param_lo = 0x00;
    const uint16_t checksum = yx_checksum(cmd, feedback, param_hi, param_lo);
    const uint8_t frame[] = {
        0x7E,
        0xFF,
        0x06,
        cmd,
        feedback,
        param_hi,
        param_lo,
        (uint8_t)(checksum >> 8),
        (uint8_t)(checksum & 0xFF),
        0xEF,
    };

    furi_hal_serial_tx(app->serial, frame, sizeof(frame));
    furi_hal_serial_tx_wait_complete(app->serial);
    furi_delay_ms(150);

    furi_hal_serial_async_rx_stop(app->serial);

    return app->probe_rx_count > 0;
}

static void draw_header(Canvas* canvas, const char* title) {
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, title);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_volume_bar(Canvas* canvas, uint8_t volume) {
    uint8_t width = (volume * 48) / 30;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 62, "VOL");
    canvas_draw_frame(canvas, 31, 55, 52, 8);

    if(width > 0) {
        canvas_draw_box(canvas, 33, 57, width, 4);
    }
}

static void draw_seek_bar(Canvas* canvas, uint8_t progress) {
    uint8_t width = (progress * 76) / 100;

    canvas_draw_frame(canvas, 26, 44, 78, 6);
    if(width > 0) {
        canvas_draw_box(canvas, 27, 45, width, 4);
    }
}

static void draw_visualizer(Canvas* canvas, uint8_t x, uint8_t y, uint8_t frame, bool invert) {
    static const uint8_t patterns[4][4] = {
        {2, 5, 3, 6},
        {6, 3, 5, 2},
        {3, 6, 2, 5},
        {5, 2, 6, 3},
    };

    const uint8_t* bars = patterns[frame % 4];

    for(uint8_t i = 0; i < 4; i++) {
        uint8_t height = bars[invert ? (3 - i) : i];
        uint8_t bar_x = x + (i * 3);
        uint8_t top_y = y - height;
        canvas_draw_box(canvas, bar_x, top_y, 2, height);
    }
}

static void draw_reel(Canvas* canvas, uint8_t x, uint8_t y, uint8_t frame) {
    canvas_draw_circle(canvas, x, y, 7);
    canvas_draw_disc(canvas, x, y, 2);

    if(frame % 2 == 0) {
        canvas_draw_line(canvas, x, y - 6, x, y - 4);
        canvas_draw_line(canvas, x, y + 4, x, y + 6);
        canvas_draw_line(canvas, x - 6, y, x - 4, y);
        canvas_draw_line(canvas, x + 4, y, x + 6, y);
    } else {
        canvas_draw_line(canvas, x - 5, y - 5, x - 3, y - 3);
        canvas_draw_line(canvas, x + 3, y + 3, x + 5, y + 5);
        canvas_draw_line(canvas, x - 5, y + 5, x - 3, y + 3);
        canvas_draw_line(canvas, x + 3, y - 3, x + 5, y - 5);
    }
}

static void draw_cassette(Canvas* canvas, WalkmanApp* app) {
    /* Compact cassette area, no track title/count text */
    canvas_draw_frame(canvas, 8, 16, 112, 25);
    canvas_draw_frame(canvas, 14, 21, 100, 14);

    draw_reel(canvas, 36, 28, app->reel_frame);
    draw_reel(canvas, 92, 28, app->reel_frame);

    canvas_draw_line(canvas, 45, 28, 83, 28);
    canvas_draw_line(canvas, 14, 35, 114, 35);
}

static void draw_transport_buttons(Canvas* canvas, bool playing) {
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 91, 62, "<");
    canvas_draw_str(canvas, 107, 62, ">");

    if(playing) {
        canvas_draw_str(canvas, 99, 62, "II");
    } else {
        canvas_draw_str(canvas, 100, 62, ">");
    }
}

static void draw_player(Canvas* canvas, WalkmanApp* app) {
    canvas_clear(canvas);

    draw_header(canvas, "WALKMAN");
    draw_cassette(canvas, app);

    draw_seek_bar(canvas, app->progress);
    draw_visualizer(canvas, 4, 51, app->reel_frame, false);
    draw_visualizer(canvas, 110, 51, app->reel_frame, true);

    draw_volume_bar(canvas, app->volume);
    draw_transport_buttons(canvas, app->playing);
}

static void draw_no_module(Canvas* canvas) {
    canvas_clear(canvas);
    draw_header(canvas, "MP3 MODULE NEEDED");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, "Attach UART MP3 module");
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, "TX/RX/GND/VCC first");
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, "Press OK to retry");
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "Long Back to exit");
}

static void draw_about(Canvas* canvas) {
    canvas_clear(canvas);
    draw_header(canvas, "ABOUT / HELP");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignBottom, "Flipper Walkman");
    canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignBottom, "UART MP3 Controller");
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignBottom, "GD3300D / HW-311");
    canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignBottom, "Rohit <RYNO>| @rynosec");

    canvas_draw_str(canvas, 3, 64, "OK Play  <> Song  ^v Vol  Back");
}

static void draw_splash(Canvas* canvas) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignBottom, "WALKMAN");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignBottom, "UART MP3 Player");
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignBottom, "by @rynosec");
}

static void draw_callback(Canvas* canvas, void* ctx) {
    WalkmanApp* app = ctx;

    if(app->screen == ScreenSplash) {
        draw_splash(canvas);
    } else if(app->screen == ScreenPlayer) {
        draw_player(canvas, app);
    } else if(app->screen == ScreenAbout) {
        draw_about(canvas);
    } else if(app->screen == ScreenNoModule) {
        draw_no_module(canvas);
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    WalkmanApp* app = ctx;
    WalkmanEvent event = {
        .type = EventTypeInput,
        .input = *input_event,
    };
    furi_message_queue_put(app->queue, &event, FuriWaitForever);
}

static void timer_callback(void* ctx) {
    WalkmanApp* app = ctx;
    WalkmanEvent event = {.type = EventTypeTick};
    furi_message_queue_put(app->queue, &event, 0);
}

int32_t flipper_walkman_app(void* p) {
    UNUSED(p);

    WalkmanApp* app = malloc(sizeof(WalkmanApp));
    app->queue = furi_message_queue_alloc(8, sizeof(WalkmanEvent));
    app->screen = ScreenSplash;
    app->playing = false;
    app->module_attached = false;
    app->probe_rx_count = 0;
    app->volume = 20;
    app->reel_frame = 0;
    app->progress = 0;
    app->splash_ticks = 0;
    app->serial = NULL;

    Gui* gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);

    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() / 4);

    app->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_hal_serial_init(app->serial, 9600);
    furi_hal_serial_enable_direction(app->serial, FuriHalSerialDirectionTx);
    furi_hal_serial_enable_direction(app->serial, FuriHalSerialDirectionRx);

    app->module_attached = detect_mp3_module(app);
    if(app->module_attached) {
        cmd_set_volume(app->serial, app->volume);
        cmd_play_first(app->serial);
        app->playing = true;
    }

    bool running = true;

    while(running) {
        WalkmanEvent event;

        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        if(event.type == EventTypeTick) {
            if(app->screen == ScreenSplash) {
                app->splash_ticks++;
                if(app->splash_ticks >= 4) {
                    app->screen = app->module_attached ? ScreenPlayer : ScreenNoModule;
                }
            }

            if(app->playing && app->screen == ScreenPlayer) {
                app->reel_frame++;
                app->progress++;
                if(app->progress > 100) app->progress = 0;
            }

            redraw(app);
            continue;
        }

        if(event.type == EventTypeInput) {
            InputKey key = event.input.key;
            InputType type = event.input.type;

            if(type == InputTypeLong && key == InputKeyBack) {
                running = false;
                continue;
            }

            if(app->screen == ScreenSplash) {
                app->screen = app->module_attached ? ScreenPlayer : ScreenNoModule;
                redraw(app);
                continue;
            }

            if(app->screen == ScreenNoModule) {
                if(type == InputTypeShort && key == InputKeyOk) {
                    app->module_attached = detect_mp3_module(app);
                    if(app->module_attached) {
                        cmd_set_volume(app->serial, app->volume);
                        cmd_play_first(app->serial);
                        app->playing = true;
                        app->progress = 0;
                        app->screen = ScreenPlayer;
                    }
                    redraw(app);
                }
                continue;
            }

            if(app->screen == ScreenAbout) {
                if(type == InputTypeShort && (key == InputKeyOk || key == InputKeyBack)) {
                    app->screen = ScreenPlayer;
                    redraw(app);
                }
                continue;
            }

            if(type == InputTypeLong && key == InputKeyOk) {
                app->screen = ScreenAbout;
                redraw(app);
                continue;
            }

            if(type == InputTypeShort) {
                if(key == InputKeyOk) {
                    if(app->playing) {
                        cmd_pause(app->serial);
                        app->playing = false;
                    } else {
                        cmd_resume(app->serial);
                        app->playing = true;
                    }
                } else if(key == InputKeyRight) {
                    cmd_next(app->serial);
                    app->progress = 0;
                    app->playing = true;
                } else if(key == InputKeyLeft) {
                    cmd_prev(app->serial);
                    app->progress = 0;
                    app->playing = true;
                } else if(key == InputKeyUp) {
                    if(app->volume < 30) app->volume++;
                    cmd_set_volume(app->serial, app->volume);
                } else if(key == InputKeyDown) {
                    if(app->volume > 0) app->volume--;
                    cmd_set_volume(app->serial, app->volume);
                } else if(key == InputKeyBack) {
                    cmd_pause(app->serial);
                    app->playing = false;
                }

                redraw(app);
            }
        }
    }

    if(app->module_attached) {
        cmd_pause(app->serial);
    }

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    furi_hal_serial_disable_direction(app->serial, FuriHalSerialDirectionTx);
    furi_hal_serial_disable_direction(app->serial, FuriHalSerialDirectionRx);
    furi_hal_serial_deinit(app->serial);
    furi_hal_serial_control_release(app->serial);

    gui_remove_view_port(gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->queue);
    free(app);

    return 0;
}
