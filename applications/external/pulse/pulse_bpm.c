#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <stdlib.h>

#include <pulse_bpm_icons.h>

#define SPLASH_MS       1800
#define WINDOW_MS       20000
#define MIN_INTERVAL_MS 250
#define MAX_BEATS       96

typedef enum {
    StateSplash,
    StateReady,
    StateLive,
    StateResult,
} AppState;

typedef struct {
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notify;
    AppState state;
    uint32_t splash_at;
    uint32_t beats[MAX_BEATS];
    uint8_t beat_count;
    uint32_t locked_bpm;
    bool running;
} PulseBpm;

static bool pulse_is_back(InputKey key) {
    return key == InputKeyBack || key == InputKeyLeft;
}

static void pulse_beep(PulseBpm* app) {
    notification_message(app->notify, &sequence_single_vibro);
}

static uint32_t pulse_elapsed_ms(const PulseBpm* app) {
    if(app->beat_count == 0) {
        return 0;
    }
    uint32_t elapsed = furi_get_tick() - app->beats[0];
    return elapsed > WINDOW_MS ? WINDOW_MS : elapsed;
}

static uint32_t pulse_bpm_from_beats(const PulseBpm* app) {
    if(app->beat_count < 2) {
        return 0;
    }
    uint32_t span = app->beats[app->beat_count - 1] - app->beats[0];
    if(span == 0) {
        return 0;
    }
    return (uint32_t)(((app->beat_count - 1) * 60000UL + span / 2) / span);
}

static void pulse_reset_measure(PulseBpm* app) {
    app->beat_count = 0;
    app->locked_bpm = 0;
}

static void pulse_lock(PulseBpm* app) {
    app->locked_bpm = pulse_bpm_from_beats(app);
    app->state = StateResult;
    pulse_beep(app);
}

static void pulse_add_beat(PulseBpm* app) {
    uint32_t now = furi_get_tick();

    if(app->beat_count == 0) {
        app->beats[0] = now;
        app->beat_count = 1;
        app->state = StateLive;
        pulse_beep(app);
        return;
    }

    if(app->beat_count >= MAX_BEATS) {
        return;
    }

    uint32_t last = app->beats[app->beat_count - 1];
    if(now - last < MIN_INTERVAL_MS) {
        return;
    }
    if(now - app->beats[0] > WINDOW_MS) {
        pulse_lock(app);
        return;
    }

    app->beats[app->beat_count++] = now;
    pulse_beep(app);
}

static void pulse_draw_splash(Canvas* canvas) {
    canvas_draw_icon(canvas, 0, 0, &I_splash_128x64);
}

static void pulse_draw_ready(Canvas* canvas) {
    canvas_draw_icon(canvas, 6, 8, &I_heart_24x24);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 36, 18, "Pulse BPM");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 36, 32, "OK on each beat");
    canvas_draw_str(canvas, 36, 44, "Locks after 20 s");
    elements_button_left(canvas, "Exit");
    elements_button_center(canvas, "Beat");
}

static void pulse_draw_live(Canvas* canvas, const PulseBpm* app) {
    uint32_t elapsed = pulse_elapsed_ms(app);
    uint32_t left_ms = WINDOW_MS - elapsed;
    uint32_t bpm = pulse_bpm_from_beats(app);

    char bpm_line[12];
    if(bpm == 0) {
        snprintf(bpm_line, sizeof(bpm_line), "--");
    } else {
        snprintf(bpm_line, sizeof(bpm_line), "%lu", (unsigned long)bpm);
    }

    char info[32];
    snprintf(
        info,
        sizeof(info),
        "%u beats  %lu s left",
        (unsigned)app->beat_count,
        (unsigned long)((left_ms + 99) / 1000));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "BPM");

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, bpm_line);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, info);

    canvas_draw_frame(canvas, 8, 46, 112, 6);
    uint8_t fill = (uint8_t)((elapsed * 110) / WINDOW_MS);
    if(fill > 0) {
        canvas_draw_box(canvas, 9, 47, fill, 4);
    }

    elements_button_left(canvas, "Cancel");
    elements_button_center(canvas, "Beat");
}

static void pulse_draw_result(Canvas* canvas, const PulseBpm* app) {
    char bpm_line[12];
    if(app->locked_bpm == 0) {
        snprintf(bpm_line, sizeof(bpm_line), "--");
    } else {
        snprintf(bpm_line, sizeof(bpm_line), "%lu", (unsigned long)app->locked_bpm);
    }

    char info[32];
    if(app->locked_bpm == 0) {
        snprintf(info, sizeof(info), "Need 2+ beats");
    } else {
        snprintf(info, sizeof(info), "%u beats  20.0 s", (unsigned)app->beat_count);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Locked BPM");

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, bpm_line);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, info);

    elements_button_left(canvas, "Exit");
    elements_button_center(canvas, "Again");
}

static void pulse_draw_callback(Canvas* canvas, void* context) {
    PulseBpm* app = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(app->state) {
    case StateSplash:
        pulse_draw_splash(canvas);
        break;
    case StateReady:
        pulse_draw_ready(canvas);
        break;
    case StateLive:
        pulse_draw_live(canvas, app);
        break;
    case StateResult:
        pulse_draw_result(canvas, app);
        break;
    }
}

static void pulse_input_callback(InputEvent* event, void* context) {
    PulseBpm* app = context;
    furi_message_queue_put(app->queue, event, FuriWaitForever);
}

static void pulse_handle_short(PulseBpm* app, InputKey key) {
    switch(app->state) {
    case StateSplash:
        if(key == InputKeyOk || pulse_is_back(key)) {
            app->state = StateReady;
        }
        break;
    case StateReady:
        if(key == InputKeyOk) {
            pulse_reset_measure(app);
            pulse_add_beat(app);
        } else if(pulse_is_back(key)) {
            app->running = false;
        }
        break;
    case StateLive:
        if(key == InputKeyOk) {
            pulse_add_beat(app);
        } else if(pulse_is_back(key)) {
            pulse_reset_measure(app);
            app->state = StateReady;
        }
        break;
    case StateResult:
        if(key == InputKeyOk) {
            pulse_reset_measure(app);
            app->state = StateReady;
        } else if(pulse_is_back(key)) {
            app->running = false;
        }
        break;
    }
}

int32_t pulse_bpm_app(void* p) {
    UNUSED(p);

    PulseBpm* app = malloc(sizeof(PulseBpm));
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->state = StateSplash;
    app->splash_at = furi_get_tick();
    app->running = true;
    pulse_reset_measure(app);

    view_port_draw_callback_set(app->view_port, pulse_draw_callback, app);
    view_port_input_callback_set(app->view_port, pulse_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent event;
    while(app->running) {
        uint32_t wait = FuriWaitForever;
        if(app->state == StateSplash || app->state == StateLive) {
            wait = 50;
        }

        FuriStatus status = furi_message_queue_get(app->queue, &event, wait);

        if(app->state == StateSplash && (furi_get_tick() - app->splash_at) >= SPLASH_MS) {
            app->state = StateReady;
        }

        if(app->state == StateLive && app->beat_count > 0 &&
           (furi_get_tick() - app->beats[0]) >= WINDOW_MS) {
            pulse_lock(app);
        }

        if(status == FuriStatusOk && event.type == InputTypeShort) {
            pulse_handle_short(app, event.key);
        }

        view_port_update(app->view_port);
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
