#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_SPLITS         50
#define MAX_DISPLAY_SPLITS 2

typedef struct {
    bool running;
    uint32_t start_tick;
    uint32_t elapsed_ms;
    uint32_t splits[MAX_SPLITS];
    uint8_t split_count;
    FuriMutex* mutex;
} StopwatchModel;

typedef struct {
    ViewPort* view_port;
    Gui* gui;
    FuriMessageQueue* event_queue;
    FuriTimer* timer;
    StopwatchModel* model;
} StopwatchApp;

typedef enum {
    EventTypeInput,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} StopwatchEvent;

static void timer_callback(void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    StopwatchEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    StopwatchEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, 0);
}

static void render_callback(Canvas* canvas, void* ctx) {
    StopwatchModel* model = ctx;
    furi_mutex_acquire(model->mutex, FuriWaitForever);

    uint32_t current_ms = model->elapsed_ms;
    if(model->running) {
        current_ms += (furi_get_tick() - model->start_tick);
    }

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontBigNumbers);
    char time_str[32];
    uint32_t sec = current_ms / 1000;
    uint32_t cs = (current_ms % 1000) / 10;

    if(sec >= 60) {
        snprintf(time_str, sizeof(time_str), "%lu:%02lu.%02lu", sec / 60, sec % 60, cs);
    } else {
        snprintf(time_str, sizeof(time_str), "%lu.%02lu", sec, cs);
    }
    canvas_draw_str(canvas, 6, 20, time_str);

    canvas_draw_line(canvas, 4, 24, 75, 24);

    canvas_set_font(canvas, FontSecondary);
    int start_idx = 0;
    if(model->split_count > MAX_DISPLAY_SPLITS) {
        start_idx = model->split_count - MAX_DISPLAY_SPLITS;
    }

    for(int i = start_idx; i < model->split_count; i++) {
        uint32_t s_ms = model->splits[i];
        uint32_t s_sec = s_ms / 1000;
        uint32_t s_cs = (s_ms % 1000) / 10;
        char split_str[32];

        if(s_sec >= 60) {
            snprintf(
                split_str,
                sizeof(split_str),
                "%d) %lu:%02lu.%02lu",
                i + 1,
                s_sec / 60,
                s_sec % 60,
                s_cs);
        } else {
            snprintf(split_str, sizeof(split_str), "%d) %lu.%02lu", i + 1, s_sec, s_cs);
        }
        int y_pos = 37 + ((i - start_idx) * 12);
        canvas_draw_str(canvas, 6, y_pos, split_str);
    }

    //  Start / Stop
    if(model->running) {
        canvas_draw_str(canvas, 4, 62, "Stop");
        canvas_draw_disc(canvas, 27, 58, 1);
        canvas_draw_circle(canvas, 27, 58, 3);
    } else {
        canvas_draw_str(canvas, 4, 62, "Start");
        canvas_draw_disc(canvas, 31, 58, 1);
        canvas_draw_circle(canvas, 31, 58, 3);
    }

    //  Split
    if(model->running) {
        canvas_draw_str(canvas, 88, 62, "Split");
        canvas_draw_line(canvas, 111, 55, 117, 55);
        canvas_draw_line(canvas, 112, 56, 116, 56);
        canvas_draw_line(canvas, 113, 57, 115, 57);
        canvas_draw_dot(canvas, 114, 58);
    } else {
        // Reset
        canvas_draw_str(canvas, 85, 62, "Reset");
        canvas_draw_dot(canvas, 114, 55);
        canvas_draw_line(canvas, 113, 56, 115, 56);
        canvas_draw_line(canvas, 112, 57, 116, 57);
        canvas_draw_line(canvas, 111, 58, 117, 58);
    }

    furi_mutex_release(model->mutex);
}

int32_t stopwatch_app_main(void* p) {
    UNUSED(p);
    StopwatchApp* app = malloc(sizeof(StopwatchApp));
    app->model = malloc(sizeof(StopwatchModel));
    app->model->running = false;
    app->model->elapsed_ms = 0;
    app->model->split_count = 0;
    app->model->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->event_queue = furi_message_queue_alloc(8, sizeof(StopwatchEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app->model);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app->event_queue);
    furi_timer_start(app->timer, furi_ms_to_ticks(30));

    StopwatchEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeInput) {
                if(event.input.type == InputTypeShort || event.input.type == InputTypeRepeat) {
                    furi_mutex_acquire(app->model->mutex, FuriWaitForever);
                    switch(event.input.key) {
                    case InputKeyOk:

                        if(app->model->running) {
                            app->model->elapsed_ms += (furi_get_tick() - app->model->start_tick);
                            app->model->running = false;
                        } else {
                            app->model->start_tick = furi_get_tick();
                            app->model->running = true;
                        }
                        break;
                    case InputKeyDown:

                        if(app->model->running && app->model->split_count < MAX_SPLITS) {
                            uint32_t current_ms = app->model->elapsed_ms +
                                                  (furi_get_tick() - app->model->start_tick);
                            app->model->splits[app->model->split_count++] = current_ms;
                        }
                        break;
                    case InputKeyUp:

                        if(!app->model->running) {
                            app->model->elapsed_ms = 0;
                            app->model->split_count = 0;
                        }
                        break;
                    case InputKeyBack:
                        running = false;
                        break;
                    default:
                        break;
                    }
                    furi_mutex_release(app->model->mutex);
                    view_port_update(app->view_port);
                }
            } else if(event.type == EventTypeTick) {
                if(app->model->running) {
                    view_port_update(app->view_port);
                }
            }
        }
    }

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->model->mutex);
    free(app->model);
    free(app);

    return 0;
}
