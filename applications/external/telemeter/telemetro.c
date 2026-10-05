#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <stdlib.h>

#include <telemetro_icons.h>

#define SOUND_SPEED_0C 331.3f
#define SOUND_SPEED_PER_C 0.606f
#define SPLASH_MS 1800
#define MIN_SAMPLE_MS 50
#define MAX_SAMPLE_MS (120 * 1000)
#define TEMP_MIN -10
#define TEMP_MAX 40

typedef enum {
    StateSplash,
    StateMenu,
    StateWaitFlash,
    StateTiming,
    StateResult,
} AppState;

typedef enum {
    ModeStorm,
    ModeCannon,
} AppMode;

typedef struct {
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notify;
    AppState state;
    AppMode mode;
    int32_t temp_c;
    uint32_t splash_at;
    uint32_t t_start;
    uint32_t t_end;
    bool running;
} Telemetro;

static float telemetro_speed(int32_t temp_c) {
    return SOUND_SPEED_0C + SOUND_SPEED_PER_C * (float)temp_c;
}

static uint32_t telemetro_elapsed_ms(const Telemetro* app) {
    uint32_t end = (app->state == StateTiming) ? furi_get_tick() : app->t_end;
    if(end < app->t_start) {
        return 0;
    }
    uint32_t ms = end - app->t_start;
    return ms > MAX_SAMPLE_MS ? MAX_SAMPLE_MS : ms;
}

static void telemetro_beep(Telemetro* app) {
    notification_message(app->notify, &sequence_single_vibro);
}

static void telemetro_draw_splash(Canvas* canvas) {
    canvas_draw_icon(canvas, 0, 0, &I_splash_128x64);
}

static void telemetro_draw_menu(Canvas* canvas, const Telemetro* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Telemeter");

    canvas_set_font(canvas, FontSecondary);
    const char* storm = (app->mode == ModeStorm) ? "> Storm" : "  Storm";
    const char* cannon = (app->mode == ModeCannon) ? "> Cannon" : "  Cannon";
    canvas_draw_str(canvas, 10, 24, storm);
    canvas_draw_str(canvas, 10, 36, cannon);

    char temp_line[24];
    snprintf(temp_line, sizeof(temp_line), "Air %ld C", (long)app->temp_c);
    canvas_draw_str(canvas, 10, 48, temp_line);

    elements_button_left(canvas, "Temp");
    elements_button_right(canvas, "Temp");
    elements_button_center(canvas, "OK");
}

static void telemetro_draw_wait(Canvas* canvas, const Telemetro* app) {
    const Icon* icon = (app->mode == ModeStorm) ? &I_lightning_24x24 : &I_cannon_24x24;
    canvas_draw_icon(canvas, 8, 10, icon);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 40, 20, app->mode == ModeStorm ? "Storm" : "Cannon");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(
        canvas, 40, 34, app->mode == ModeStorm ? "OK on flash" : "OK on flame");
    canvas_draw_str(canvas, 40, 46, "Starts the timer");

    elements_button_left(canvas, "Back");
    elements_button_center(canvas, "OK");
}

static void telemetro_draw_timing(Canvas* canvas, const Telemetro* app) {
    uint32_t ms = telemetro_elapsed_ms(app);
    char big[16];
    snprintf(big, sizeof(big), "%lu.%lu", (unsigned long)(ms / 1000), (unsigned long)((ms / 100) % 10));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Listening...");

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, big);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        64,
        40,
        AlignCenter,
        AlignTop,
        app->mode == ModeStorm ? "OK on thunder" : "OK on boom");

    elements_button_left(canvas, "Cancel");
    elements_button_center(canvas, "OK");
}

static void telemetro_draw_result(Canvas* canvas, const Telemetro* app) {
    uint32_t ms = telemetro_elapsed_ms(app);
    float seconds = ms / 1000.0f;
    float meters = seconds * telemetro_speed(app->temp_c);
    unsigned meters_i = (unsigned)(meters + 0.5f);
    unsigned km_int = meters_i / 1000;
    unsigned km_frac = (meters_i % 1000) / 10;

    char meters_line[16];
    snprintf(meters_line, sizeof(meters_line), "%u", meters_i);

    char detail[32];
    snprintf(
        detail,
        sizeof(detail),
        "%u.%02u km  %lu.%02lu s",
        km_int,
        km_frac,
        (unsigned long)(ms / 1000),
        (unsigned long)((ms / 10) % 100));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Distance (m)");

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, meters_line);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, detail);

    elements_button_left(canvas, "Back");
    elements_button_center(canvas, "Again");
}

static void telemetro_draw_callback(Canvas* canvas, void* context) {
    Telemetro* app = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(app->state) {
    case StateSplash:
        telemetro_draw_splash(canvas);
        break;
    case StateMenu:
        telemetro_draw_menu(canvas, app);
        break;
    case StateWaitFlash:
        telemetro_draw_wait(canvas, app);
        break;
    case StateTiming:
        telemetro_draw_timing(canvas, app);
        break;
    case StateResult:
        telemetro_draw_result(canvas, app);
        break;
    }
}

static void telemetro_input_callback(InputEvent* event, void* context) {
    Telemetro* app = context;
    furi_message_queue_put(app->queue, event, FuriWaitForever);
}

static void telemetro_start_timing(Telemetro* app) {
    app->t_start = furi_get_tick();
    app->t_end = app->t_start;
    app->state = StateTiming;
    telemetro_beep(app);
}

static void telemetro_stop_timing(Telemetro* app) {
    app->t_end = furi_get_tick();
    if(telemetro_elapsed_ms(app) < MIN_SAMPLE_MS) {
        return;
    }
    app->state = StateResult;
    telemetro_beep(app);
}

static bool telemetro_is_back(InputKey key) {
    /* elements_button_left() is the Left key; Back is the dedicated return button. */
    return key == InputKeyBack || key == InputKeyLeft;
}

static void telemetro_handle_short(Telemetro* app, InputKey key) {
    switch(app->state) {
    case StateSplash:
        if(key == InputKeyOk || telemetro_is_back(key)) {
            app->state = StateMenu;
        }
        break;
    case StateMenu:
        if(key == InputKeyUp || key == InputKeyDown) {
            app->mode = (app->mode == ModeStorm) ? ModeCannon : ModeStorm;
        } else if(key == InputKeyLeft) {
            if(app->temp_c > TEMP_MIN) app->temp_c--;
        } else if(key == InputKeyRight) {
            if(app->temp_c < TEMP_MAX) app->temp_c++;
        } else if(key == InputKeyOk) {
            app->state = StateWaitFlash;
        } else if(key == InputKeyBack) {
            app->running = false;
        }
        break;
    case StateWaitFlash:
        if(key == InputKeyOk) {
            telemetro_start_timing(app);
        } else if(telemetro_is_back(key)) {
            app->state = StateMenu;
        }
        break;
    case StateTiming:
        if(key == InputKeyOk) {
            telemetro_stop_timing(app);
        } else if(telemetro_is_back(key)) {
            app->state = StateWaitFlash;
        }
        break;
    case StateResult:
        if(key == InputKeyOk) {
            app->state = StateWaitFlash;
        } else if(telemetro_is_back(key)) {
            app->state = StateMenu;
        }
        break;
    }
}

int32_t telemetro_app(void* p) {
    UNUSED(p);

    Telemetro* app = malloc(sizeof(Telemetro));
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->state = StateSplash;
    app->mode = ModeStorm;
    app->temp_c = 20;
    app->splash_at = furi_get_tick();
    app->running = true;

    view_port_draw_callback_set(app->view_port, telemetro_draw_callback, app);
    view_port_input_callback_set(app->view_port, telemetro_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent event;
    while(app->running) {
        uint32_t wait = FuriWaitForever;
        if(app->state == StateSplash || app->state == StateTiming) {
            wait = 50;
        }

        FuriStatus status = furi_message_queue_get(app->queue, &event, wait);

        if(app->state == StateSplash && (furi_get_tick() - app->splash_at) >= SPLASH_MS) {
            app->state = StateMenu;
        }

        if(app->state == StateTiming && telemetro_elapsed_ms(app) >= MAX_SAMPLE_MS) {
            app->t_end = app->t_start + MAX_SAMPLE_MS;
            app->state = StateResult;
        }

        if(status == FuriStatusOk && event.type == InputTypeShort) {
            telemetro_handle_short(app, event.key);
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
