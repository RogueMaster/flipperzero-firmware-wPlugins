#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
//#include <dialogs/dialogs.h>
#include <storage/storage.h> //to store test results
#include <flipper_format/flipper_format.h>
#include <notification/notification_messages.h>

#define TAG          "ReactionTime"
#define MIN_DELAY_MS 1500
#define MAX_DELAY_MS 5000
#define STATS_PATH   APP_DATA_PATH("stats.txt")
#define NICE_MS      130
#define VERSION      "v1.0.0"

typedef enum {
    ScreenStartup,
    ScreenWaitSignal,
    ScreenGo,
    ScreenTooEarly,
    ScreenResult,
    ScreenAbout,
} Screen;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriMutex* mutex;
    NotificationApp* notifications;

    Screen screen;
    uint32_t wait_start;
    uint32_t wait_duration;
    uint32_t go_time;
    uint32_t reaction_ms;
    uint32_t total_ms;
    uint32_t count;

} ReactionApp;
static const NotificationSequence sequence_nice = {
    &message_red_255,   &message_green_255,

    &message_note_g5,   &message_delay_50,
    &message_sound_off, &message_delay_25,
    &message_note_g5,   &message_delay_50,
    &message_sound_off, &message_delay_25,
    &message_note_g5,   &message_delay_50,
    &message_sound_off, &message_delay_25,
    &message_note_c6,   &message_delay_250,
    &message_delay_100, &message_sound_off,
    &message_delay_100,

    &message_note_c6,   &message_delay_50,
    &message_sound_off, &message_delay_25,
    &message_note_c6,   &message_delay_50,
    &message_sound_off, &message_delay_25,
    &message_note_c6,   &message_delay_50,
    &message_sound_off, &message_delay_25,
    &message_note_e6,   &message_delay_250,
    &message_delay_100, &message_sound_off,
    &message_delay_100,

    &message_note_c6,   &message_delay_100,
    &message_note_e6,   &message_delay_100,
    &message_note_g6,   &message_delay_100,
    &message_sound_off, &message_delay_25,
    &message_note_g6,   &message_delay_50,
    &message_sound_off, &message_delay_25,
    &message_note_g6,   &message_delay_50,
    &message_sound_off, &message_delay_25,

    &message_note_c7,   &message_delay_500,
    &message_delay_250, &message_sound_off,
    &message_vibro_on,  &message_delay_100,
    &message_vibro_off, &message_red_0,
    &message_green_0,   NULL,
};

static void load_stats(ReactionApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_existing(ff, STATS_PATH)) {
        flipper_format_read_uint32(ff, "Total", &app->total_ms, 1);
        flipper_format_read_uint32(ff, "Count", &app->count, 1);
    }

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

static void save_stats(ReactionApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(ff, STATS_PATH)) {
        flipper_format_write_uint32(ff, "Total", &app->total_ms, 1);
        flipper_format_write_uint32(ff, "Count", &app->count, 1);
    }

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}
static uint32_t get_average(ReactionApp* app) {
    if(app->count == 0) {
        return 0;
    }
    return app->total_ms / app->count;
}
static void draw_splash(Canvas* canvas, ReactionApp* app) {
    char buf[40];
    uint32_t avg = get_average(app);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Reaction Time Game");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, "Test your reaction time!");

    if(avg > 0) {
        snprintf(buf, sizeof(buf), "Average: %lu ms", avg);
        //canvas_draw_str(canvas, 64, 32, buf);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, buf);
    }
    //canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "Reset");
    elements_button_center(canvas, "Start");
    elements_button_right(canvas, "About");
}

static void draw_wait(Canvas* canvas, ReactionApp* app) {
    UNUSED(app);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Wait...");
}

static void draw_go(Canvas* canvas, ReactionApp* app) {
    UNUSED(app);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "PRESS!!!");
}

static void draw_too_early(Canvas* canvas, ReactionApp* app) {
    UNUSED(app);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Failed...");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, "You pressed too early");
    elements_button_center(canvas, "Continue");
}

static void draw_back_icon(Canvas* canvas, int32_t x, int32_t y) {
    // nuolenkärki vasemmalle
    canvas_draw_line(canvas, x, y + 2, x + 2, y);
    canvas_draw_line(canvas, x, y + 2, x + 2, y + 4);
    // varsi oikealle
    canvas_draw_line(canvas, x, y + 2, x + 6, y + 2);
    // kaarre alas
    canvas_draw_line(canvas, x + 7, y + 3, x + 7, y + 5);
    // alaviiva takaisin vasemmalle
    canvas_draw_line(canvas, x + 6, y + 6, x + 3, y + 6);
}

static void draw_reaction_ms(Canvas* canvas, ReactionApp* app) {
    char buf[40];
    uint32_t reaction_time_ms = app->reaction_ms;

    if(reaction_time_ms < NICE_MS) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Nice!");
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Your reaction time was:");
    snprintf(buf, sizeof(buf), "%lu ms", reaction_time_ms);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, buf);
    elements_button_center(canvas, "Again");
    draw_back_icon(canvas, 2, 55);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 12, 63, AlignLeft, AlignBottom, "Menu");
    //canvas_draw_str(canvas, 64, 32, buf);
}

static void draw_about(Canvas* canvas, ReactionApp* app) {
    UNUSED(app);
    char buf[40];
    canvas_set_font(canvas, FontPrimary);
    snprintf(buf, sizeof(buf), "Reaction Time %s", VERSION);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "Developed by luvrltop");
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, "Github: luvrltop");
    canvas_draw_str_aligned(canvas, 5, 32, AlignLeft, AlignTop, "Hint:");
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "Try getting a score under");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "130ms!");

    //scrollcanvas
}

static void draw_callback(Canvas* canvas, void* ctx) {
    ReactionApp* app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever); // lock
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch(app->screen) {
    case ScreenStartup:
        draw_splash(canvas, app);
        break;

    case ScreenWaitSignal:
        draw_wait(canvas, app);
        break;

    case ScreenGo:
        draw_go(canvas, app);
        break;

    case ScreenTooEarly:
        draw_too_early(canvas, app);
        break;

    case ScreenResult:
        draw_reaction_ms(canvas, app);
        break;
    case ScreenAbout:
        draw_about(canvas, app);
        break;
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

static void start_waiting(ReactionApp* app) {
    app->wait_duration = MIN_DELAY_MS + (furi_hal_random_get() % (MAX_DELAY_MS - MIN_DELAY_MS));
    app->wait_start = furi_get_tick();
    app->screen = ScreenWaitSignal;
}

//create app
static ReactionApp* app_alloc(void) {
    ReactionApp* app = malloc(sizeof(ReactionApp));

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->screen = ScreenStartup;

    app->total_ms = 0;
    app->count = 0;
    load_stats(app);

    return app;
}

static void app_free(ReactionApp* app) {
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_enabled_set(app->view_port, false);
    view_port_free(app->view_port);
    furi_mutex_free(app->mutex);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    free(app);
}
static void clear_stats(ReactionApp* app) {
    app->total_ms = 0;
    app->count = 0;
}
//main
int32_t reaction_time_app(void* p) {
    UNUSED(p);
    ReactionApp* app = app_alloc();

    InputEvent event;
    bool running = true;

    while(running) {
        //wait btn for 10 ms max. returns FuriStatusOk if button
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 10);

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if(status == FuriStatusOk && event.type == InputTypePress) {
            if(event.key == InputKeyBack) {
                //if app->screen is at startup: close app
                if(app->screen == ScreenStartup) {
                    running = false;
                } else { //else go back to startup to check e.g. average score
                    app->screen = ScreenStartup;
                }

            } else if(event.key == InputKeyLeft) {
                //if left button pressed and app->screen is at startup
                if(app->screen == ScreenStartup) {
                    clear_stats(app);
                    save_stats(app);
                }

            } else if(event.key == InputKeyRight && app->screen == ScreenStartup) {
                app->screen = ScreenAbout;

            } else if(event.key == InputKeyBack && app->screen == ScreenAbout) {
                app->screen = ScreenStartup;

            } else if(event.key == InputKeyOk) {
                //Ok button pressed: what happens
                switch(app->screen) {
                case ScreenStartup:
                    start_waiting(app);
                    break;
                case ScreenWaitSignal:
                    app->screen = ScreenTooEarly;
                    break;
                case ScreenGo:
                    app->reaction_ms = furi_get_tick() - app->go_time;
                    app->total_ms += app->reaction_ms;
                    app->count++;
                    save_stats(app);
                    if(app->reaction_ms < NICE_MS) {
                        notification_message(app->notifications, &sequence_nice);
                    }

                    app->screen = ScreenResult;
                    break;
                case ScreenTooEarly:
                    start_waiting(app);
                    break;
                case ScreenResult:

                    start_waiting(app); //new cycle
                    break;
                case ScreenAbout:
                    break;
                }
            }
        }

        //move to ScreenGo aka. game view after random 1500-5000ms wait is over
        if(app->screen == ScreenWaitSignal &&
           furi_get_tick() - app->wait_start >= app->wait_duration) {
            app->screen = ScreenGo;
            app->go_time = furi_get_tick();
            notification_message(app->notifications, &sequence_success);
        }

        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }

    app_free(app);
    return 0;
}
