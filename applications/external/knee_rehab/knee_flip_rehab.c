#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define KNEE_FLIP_SIMPLE_TIMER_SECONDS (20U * 60U)
#define KNEE_FLIP_QUAD_HOLD_SECONDS    5U
#define KNEE_FLIP_QUAD_REST_SECONDS    10U
#define KNEE_FLIP_QUAD_REPS            10U
#define KNEE_FLIP_HEEL_REPS            10U

#define KNEE_FLIP_DISCLAIMER                      \
    "KneeFlip Rehab is a timer and counter tool " \
    "only. It does not provide medical advice, "  \
    "diagnosis, or treatment. Always follow the " \
    "recovery plan provided by your doctor, "     \
    "surgeon, or physical therapist."

typedef enum {
    KneeFlipViewMenu,
    KneeFlipViewText,
    KneeFlipViewBlock,
} KneeFlipView;

typedef enum {
    KneeFlipMenuIceTimer,
    KneeFlipMenuElevationTimer,
    KneeFlipMenuQuadSets,
    KneeFlipMenuHeelSlides,
    KneeFlipMenuAbout,
} KneeFlipMenuItem;

typedef struct {
    const char* title;
    uint32_t duration_seconds;
    uint32_t remaining_seconds;
    bool running;
    bool paused;
    bool completed;
    bool cancelled;
} KneeFlipSimpleTimer;

typedef enum {
    KneeFlipQuadPhaseHold,
    KneeFlipQuadPhaseRest,
    KneeFlipQuadPhaseComplete,
} KneeFlipQuadPhase;

typedef struct {
    uint8_t rep;
    uint8_t total_reps;
    uint32_t phase_remaining_seconds;
    KneeFlipQuadPhase phase;
    bool running;
    bool paused;
    bool completed;
    bool cancelled;
} KneeFlipQuadTimer;

typedef struct {
    uint8_t reps;
    uint8_t target_reps;
    bool completed;
} KneeFlipHeelCounter;

typedef enum {
    KneeFlipBlockNone,
    KneeFlipBlockSimpleTimer,
    KneeFlipBlockQuadTimer,
    KneeFlipBlockHeelCounter,
} KneeFlipBlock;

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    TextBox* text_box;
    View* block_view;
    KneeFlipBlock active_block;
    KneeFlipSimpleTimer simple_timer;
    KneeFlipQuadTimer quad_timer;
    KneeFlipHeelCounter heel_counter;
    bool showing_text;
    bool showing_block;
    bool views_added;
    bool block_model_allocated;
} KneeFlipApp;

static void knee_flip_notify(NotificationApp* notification, const NotificationSequence* sequence) {
    if(notification && sequence) {
        notification_message(notification, sequence);
    }
}

static void knee_flip_reset_simple_timer(
    KneeFlipSimpleTimer* timer,
    const char* title,
    uint32_t duration_seconds) {
    furi_assert(timer);
    furi_assert(title);

    timer->title = title;
    timer->duration_seconds = duration_seconds;
    timer->remaining_seconds = duration_seconds;
    timer->running = false;
    timer->paused = false;
    timer->completed = false;
    timer->cancelled = false;
}

static void knee_flip_reset_quad_timer(KneeFlipQuadTimer* timer) {
    furi_assert(timer);

    timer->rep = 1;
    timer->total_reps = KNEE_FLIP_QUAD_REPS;
    timer->phase_remaining_seconds = KNEE_FLIP_QUAD_HOLD_SECONDS;
    timer->phase = KneeFlipQuadPhaseHold;
    timer->running = false;
    timer->paused = false;
    timer->completed = false;
    timer->cancelled = false;
}

static void knee_flip_reset_heel_counter(KneeFlipHeelCounter* counter) {
    furi_assert(counter);

    counter->reps = 0;
    counter->target_reps = KNEE_FLIP_HEEL_REPS;
    counter->completed = false;
}

static void knee_flip_format_mm_ss(char* buffer, size_t buffer_size, uint32_t seconds) {
    if(buffer && buffer_size > 0) {
        snprintf(
            buffer,
            buffer_size,
            "%02lu:%02lu",
            (unsigned long)(seconds / 60U),
            (unsigned long)(seconds % 60U));
    }
}

static void knee_flip_return_to_menu(KneeFlipApp* app) {
    furi_assert(app);

    app->showing_text = false;
    app->showing_block = false;
    app->active_block = KneeFlipBlockNone;
    view_dispatcher_switch_to_view(app->view_dispatcher, KneeFlipViewMenu);
}

static void knee_flip_refresh_block_view(KneeFlipApp* app) {
    if(!app || !app->block_view) {
        return;
    }

    KneeFlipApp** block_model = view_get_model(app->block_view);
    if(block_model) {
        *block_model = app;
        view_commit_model(app->block_view, true);
    }
}

static void knee_flip_show_text(KneeFlipApp* app, const char* text) {
    furi_assert(app);
    furi_assert(text);

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, text);
    app->showing_text = true;
    app->showing_block = false;
    view_dispatcher_switch_to_view(app->view_dispatcher, KneeFlipViewText);
}

static void knee_flip_show_block(KneeFlipApp* app, KneeFlipBlock block) {
    furi_assert(app);

    app->showing_text = false;
    app->showing_block = true;
    app->active_block = block;
    view_dispatcher_switch_to_view(app->view_dispatcher, KneeFlipViewBlock);
}

static void knee_flip_start_simple_timer(KneeFlipApp* app, const char* title) {
    furi_assert(app);
    furi_assert(title);

    knee_flip_reset_simple_timer(&app->simple_timer, title, KNEE_FLIP_SIMPLE_TIMER_SECONDS);
    knee_flip_show_block(app, KneeFlipBlockSimpleTimer);
}

static void knee_flip_cancel_active_block(KneeFlipApp* app) {
    furi_assert(app);

    if(app->active_block == KneeFlipBlockSimpleTimer) {
        app->simple_timer.running = false;
        app->simple_timer.paused = false;
        app->simple_timer.cancelled = true;
    } else if(app->active_block == KneeFlipBlockQuadTimer) {
        app->quad_timer.running = false;
        app->quad_timer.paused = false;
        app->quad_timer.cancelled = true;
    } else if(app->active_block == KneeFlipBlockHeelCounter) {
        knee_flip_return_to_menu(app);
    }
}

static void knee_flip_menu_callback(void* context, uint32_t index) {
    furi_assert(context);
    KneeFlipApp* app = context;

    switch(index) {
    case KneeFlipMenuIceTimer:
        knee_flip_start_simple_timer(app, "Ice Timer");
        break;
    case KneeFlipMenuElevationTimer:
        knee_flip_start_simple_timer(app, "Elevation");
        break;
    case KneeFlipMenuQuadSets:
        knee_flip_reset_quad_timer(&app->quad_timer);
        knee_flip_show_block(app, KneeFlipBlockQuadTimer);
        break;
    case KneeFlipMenuHeelSlides:
        knee_flip_reset_heel_counter(&app->heel_counter);
        knee_flip_show_block(app, KneeFlipBlockHeelCounter);
        break;
    case KneeFlipMenuAbout:
        knee_flip_show_text(app, "KneeFlip Rehab\n\n" KNEE_FLIP_DISCLAIMER);
        break;
    default:
        knee_flip_show_text(app, "KneeFlip Rehab\n\nUnknown menu item.");
        break;
    }
}

static void
    knee_flip_draw_footer(Canvas* canvas, const char* left, const char* center, const char* right) {
    furi_assert(canvas);

    canvas_set_font(canvas, FontSecondary);
    if(left) {
        canvas_draw_str(canvas, 2, 63, left);
    }
    if(center) {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, center);
    }
    if(right) {
        canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, right);
    }
}

static uint8_t knee_flip_timer_progress_stage(const KneeFlipSimpleTimer* timer) {
    furi_assert(timer);

    if(timer->completed || timer->remaining_seconds == 0) {
        return 5;
    }
    if(timer->duration_seconds == 0) {
        return 1;
    }

    uint32_t elapsed = timer->duration_seconds - timer->remaining_seconds;
    uint32_t percent = (elapsed * 100U) / timer->duration_seconds;
    if(percent < 25U) {
        return 1;
    } else if(percent < 50U) {
        return 2;
    } else if(percent < 75U) {
        return 3;
    }
    return 4;
}

static void knee_flip_draw_penguin(Canvas* canvas, int32_t x, int32_t y) {
    furi_assert(canvas);

    canvas_draw_circle(canvas, x, y, 6);
    canvas_draw_dot(canvas, x - 2, y - 1);
    canvas_draw_dot(canvas, x + 2, y - 1);
    canvas_draw_line(canvas, x - 2, y + 2, x, y + 4);
    canvas_draw_line(canvas, x + 2, y + 2, x, y + 4);
    canvas_draw_line(canvas, x - 8, y + 8, x - 3, y + 14);
    canvas_draw_line(canvas, x + 8, y + 8, x + 3, y + 14);
    canvas_draw_line(canvas, x - 5, y + 17, x + 5, y + 17);
    canvas_draw_line(canvas, x - 8, y + 19, x - 1, y + 19);
    canvas_draw_line(canvas, x + 1, y + 19, x + 8, y + 19);
}

static void knee_flip_draw_ice_block(Canvas* canvas, uint8_t stage) {
    furi_assert(canvas);

    canvas_draw_frame(canvas, 6, 15, 52, 28);
    canvas_draw_line(canvas, 10, 15, 15, 10);
    canvas_draw_line(canvas, 15, 10, 49, 10);
    canvas_draw_line(canvas, 49, 10, 58, 15);
    canvas_draw_line(canvas, 10, 43, 15, 48);
    canvas_draw_line(canvas, 15, 48, 49, 48);
    canvas_draw_line(canvas, 49, 48, 58, 43);

    for(uint8_t i = 0; i < 6; i++) {
        canvas_draw_dot(canvas, 14 + (i * 7), 20 + ((i % 2) * 6));
        canvas_draw_dot(canvas, 12 + (i * 7), 35 - ((i % 2) * 5));
    }

    if(stage >= 2) {
        canvas_draw_line(canvas, 29, 15, 24, 24);
        canvas_draw_line(canvas, 24, 24, 31, 30);
        canvas_draw_line(canvas, 35, 11, 31, 20);
    }
    if(stage >= 3) {
        canvas_draw_box(canvas, 24, 20, 16, 15);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 25, 21, 14, 13);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_line(canvas, 42, 22, 49, 18);
        canvas_draw_line(canvas, 17, 38, 25, 42);
    }
    if(stage >= 4) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 18, 16, 30, 24);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_line(canvas, 8, 43, 55, 43);
        canvas_draw_line(canvas, 14, 48, 50, 48);
    }
}

static void knee_flip_draw_ice_timer(Canvas* canvas, const KneeFlipSimpleTimer* timer) {
    char remaining[16];
    const char* status = "OK Start";
    uint8_t stage = knee_flip_timer_progress_stage(timer);

    furi_assert(canvas);
    furi_assert(timer);

    knee_flip_format_mm_ss(remaining, sizeof(remaining), timer->remaining_seconds);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 8, "Ice Timer");

    if(stage < 5) {
        knee_flip_draw_ice_block(canvas, stage);
        int32_t penguin_y = 27;
        if(stage == 2) {
            penguin_y = 24;
        } else if(stage == 3) {
            penguin_y = 19;
        } else if(stage == 4) {
            penguin_y = 12;
        }
        knee_flip_draw_penguin(canvas, 32, penguin_y);
    } else {
        knee_flip_draw_penguin(canvas, 32, 13);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 73, 32, "DONE");
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 74, 18, "ICE");
    canvas_draw_str(canvas, 74, 32, remaining);
    canvas_set_font(canvas, FontSecondary);

    if(timer->completed) {
        canvas_draw_str(canvas, 74, 45, "Complete");
        status = "OK Menu";
    } else if(timer->cancelled) {
        canvas_draw_str(canvas, 74, 45, "Cancelled");
        status = "OK Menu";
    } else if(timer->paused) {
        canvas_draw_str(canvas, 74, 45, "Paused");
        status = "OK Resume";
    } else if(timer->running) {
        canvas_draw_str(canvas, 74, 45, "Running");
        status = "OK Pause";
    } else {
        canvas_draw_str(canvas, 74, 45, "Clinician plan");
    }

    knee_flip_draw_footer(canvas, "Back", status, NULL);
}

static void knee_flip_draw_simple_timer(Canvas* canvas, const KneeFlipSimpleTimer* timer) {
    char remaining[16];
    const char* status = "OK Start";

    furi_assert(canvas);
    furi_assert(timer);

    knee_flip_format_mm_ss(remaining, sizeof(remaining), timer->remaining_seconds);

    if(timer->title && strcmp(timer->title, "Ice Timer") == 0) {
        knee_flip_draw_ice_timer(canvas, timer);
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, timer->title ? timer->title : "Timer");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, remaining);

    canvas_set_font(canvas, FontSecondary);
    if(timer->completed) {
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, "Timer complete");
        status = "OK Menu";
    } else if(timer->cancelled) {
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, "Session cancelled");
        status = "OK Menu";
    } else if(timer->paused) {
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, "Paused");
        status = "OK Resume";
    } else if(timer->running) {
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, "Running");
        status = "OK Pause";
    } else {
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, "Follow clinician plan");
    }

    knee_flip_draw_footer(canvas, "Back", status, NULL);
}

static void knee_flip_draw_quad_timer(Canvas* canvas, const KneeFlipQuadTimer* timer) {
    char rep_text[16];
    char phase_time[16];
    const char* phase = "HOLD";
    const char* status = "OK Start";

    furi_assert(canvas);
    furi_assert(timer);

    if(timer->phase == KneeFlipQuadPhaseRest) {
        phase = "REST";
    } else if(timer->phase == KneeFlipQuadPhaseComplete) {
        phase = "COMPLETE";
    }

    snprintf(rep_text, sizeof(rep_text), "Rep %u / %u", timer->rep, timer->total_reps);
    snprintf(phase_time, sizeof(phase_time), "%s %lus", phase, timer->phase_remaining_seconds);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Quad Sets");
    canvas_draw_str_aligned(canvas, 126, 12, AlignRight, AlignBottom, rep_text);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, phase);
    canvas_set_font(canvas, FontSecondary);
    if(timer->cancelled) {
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, "Session cancelled");
    } else {
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, phase_time);
    }

    if(timer->completed) {
        status = "OK Menu";
    } else if(timer->cancelled) {
        status = "OK Menu";
    } else if(timer->paused) {
        status = "OK Resume";
    } else if(timer->running) {
        status = "OK Pause";
    }

    knee_flip_draw_footer(canvas, "Back", status, NULL);
}

static void knee_flip_draw_heel_counter(Canvas* canvas, const KneeFlipHeelCounter* counter) {
    char rep_text[16];

    furi_assert(canvas);
    furi_assert(counter);

    snprintf(rep_text, sizeof(rep_text), "Rep %u / %u", counter->reps, counter->target_reps);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Heel Slides");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, rep_text);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 50, AlignCenter, AlignBottom, counter->completed ? "Complete" : "Manual count");

    knee_flip_draw_footer(canvas, "Back", counter->completed ? "OK Reset" : "OK +1", "Left Reset");
}

static void knee_flip_block_draw_callback(Canvas* canvas, void* model) {
    KneeFlipApp* app = NULL;
    if(model) {
        app = *((KneeFlipApp**)model);
    }

    if(!canvas || !app) {
        return;
    }

    canvas_clear(canvas);
    if(app->active_block == KneeFlipBlockSimpleTimer) {
        knee_flip_draw_simple_timer(canvas, &app->simple_timer);
    } else if(app->active_block == KneeFlipBlockQuadTimer) {
        knee_flip_draw_quad_timer(canvas, &app->quad_timer);
    } else if(app->active_block == KneeFlipBlockHeelCounter) {
        knee_flip_draw_heel_counter(canvas, &app->heel_counter);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "KneeFlip Rehab");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "No active session");
        knee_flip_draw_footer(canvas, "Back", "OK Menu", NULL);
    }
}

static void knee_flip_handle_simple_timer_input(KneeFlipApp* app, const InputEvent* event) {
    furi_assert(app);
    furi_assert(event);

    KneeFlipSimpleTimer* timer = &app->simple_timer;
    if(event->key == InputKeyOk) {
        if(timer->completed || timer->cancelled) {
            knee_flip_return_to_menu(app);
        } else if(timer->running) {
            timer->running = false;
            timer->paused = true;
        } else {
            timer->running = true;
            timer->paused = false;
        }
    } else if(event->key == InputKeyBack) {
        if(timer->completed || timer->cancelled || (!timer->running && !timer->paused)) {
            knee_flip_return_to_menu(app);
        } else {
            knee_flip_cancel_active_block(app);
        }
    }
}

static void knee_flip_handle_quad_timer_input(KneeFlipApp* app, const InputEvent* event) {
    furi_assert(app);
    furi_assert(event);

    KneeFlipQuadTimer* timer = &app->quad_timer;
    if(event->key == InputKeyOk) {
        if(timer->completed || timer->cancelled) {
            knee_flip_return_to_menu(app);
        } else if(timer->running) {
            timer->running = false;
            timer->paused = true;
        } else {
            timer->running = true;
            timer->paused = false;
        }
    } else if(event->key == InputKeyBack) {
        if(timer->completed || timer->cancelled || (!timer->running && !timer->paused)) {
            knee_flip_return_to_menu(app);
        } else {
            knee_flip_cancel_active_block(app);
        }
    }
}

static void knee_flip_handle_heel_counter_input(KneeFlipApp* app, const InputEvent* event) {
    furi_assert(app);
    furi_assert(event);

    KneeFlipHeelCounter* counter = &app->heel_counter;
    if(event->key == InputKeyBack) {
        knee_flip_return_to_menu(app);
    } else if(event->key == InputKeyLeft) {
        knee_flip_reset_heel_counter(counter);
    } else if(event->key == InputKeyOk) {
        if(counter->completed) {
            knee_flip_reset_heel_counter(counter);
        } else if(counter->reps < counter->target_reps) {
            counter->reps++;
            if(counter->reps >= counter->target_reps) {
                counter->completed = true;
                knee_flip_notify(app->notification, &sequence_success);
            }
        }
    }
}

static bool knee_flip_block_input_callback(InputEvent* event, void* context) {
    KneeFlipApp* app = context;
    if(!event || !app || event->type != InputTypeShort) {
        return false;
    }

    if(app->active_block == KneeFlipBlockSimpleTimer) {
        knee_flip_handle_simple_timer_input(app, event);
    } else if(app->active_block == KneeFlipBlockQuadTimer) {
        knee_flip_handle_quad_timer_input(app, event);
    } else if(app->active_block == KneeFlipBlockHeelCounter) {
        knee_flip_handle_heel_counter_input(app, event);
    } else if(event->key == InputKeyBack || event->key == InputKeyOk) {
        knee_flip_return_to_menu(app);
    }

    knee_flip_refresh_block_view(app);
    return true;
}

static void knee_flip_tick_simple_timer(KneeFlipApp* app) {
    KneeFlipSimpleTimer* timer = &app->simple_timer;
    if(!timer->running || timer->paused || timer->completed || timer->cancelled) {
        return;
    }

    if(timer->remaining_seconds > 0) {
        timer->remaining_seconds--;
    }

    if(timer->remaining_seconds == 0) {
        timer->running = false;
        timer->completed = true;
        knee_flip_notify(app->notification, &sequence_success);
    }
}

static void knee_flip_tick_quad_timer(KneeFlipApp* app) {
    KneeFlipQuadTimer* timer = &app->quad_timer;
    if(!timer->running || timer->paused || timer->completed || timer->cancelled) {
        return;
    }

    if(timer->phase_remaining_seconds > 0) {
        timer->phase_remaining_seconds--;
    }

    if(timer->phase_remaining_seconds > 0) {
        return;
    }

    if(timer->phase == KneeFlipQuadPhaseHold) {
        timer->phase = KneeFlipQuadPhaseRest;
        timer->phase_remaining_seconds = KNEE_FLIP_QUAD_REST_SECONDS;
        knee_flip_notify(app->notification, &sequence_single_vibro);
    } else if(timer->phase == KneeFlipQuadPhaseRest && timer->rep < timer->total_reps) {
        timer->rep++;
        timer->phase = KneeFlipQuadPhaseHold;
        timer->phase_remaining_seconds = KNEE_FLIP_QUAD_HOLD_SECONDS;
        knee_flip_notify(app->notification, &sequence_single_vibro);
    } else {
        timer->phase = KneeFlipQuadPhaseComplete;
        timer->running = false;
        timer->completed = true;
        knee_flip_notify(app->notification, &sequence_success);
    }
}

static void knee_flip_tick_callback(void* context) {
    KneeFlipApp* app = context;
    if(!app || !app->showing_block || !app->block_view) {
        return;
    }

    if(app->active_block == KneeFlipBlockSimpleTimer) {
        knee_flip_tick_simple_timer(app);
    } else if(app->active_block == KneeFlipBlockQuadTimer) {
        knee_flip_tick_quad_timer(app);
    }

    knee_flip_refresh_block_view(app);
}

static bool knee_flip_navigation_callback(void* context) {
    furi_assert(context);
    KneeFlipApp* app = context;

    if(app->showing_text) {
        app->showing_text = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, KneeFlipViewMenu);
        return true;
    }

    if(app->showing_block) {
        knee_flip_cancel_active_block(app);
        if(app->active_block != KneeFlipBlockHeelCounter) {
            knee_flip_refresh_block_view(app);
        }
        return true;
    }

    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static KneeFlipApp* knee_flip_app_alloc(void) {
    KneeFlipApp* app = malloc(sizeof(KneeFlipApp));
    if(!app) {
        return NULL;
    }

    app->gui = NULL;
    app->notification = NULL;
    app->view_dispatcher = NULL;
    app->menu = NULL;
    app->text_box = NULL;
    app->block_view = NULL;
    app->active_block = KneeFlipBlockNone;
    app->showing_text = false;
    app->showing_block = false;
    app->views_added = false;
    app->block_model_allocated = false;
    knee_flip_reset_simple_timer(&app->simple_timer, "Timer", KNEE_FLIP_SIMPLE_TIMER_SECONDS);
    knee_flip_reset_quad_timer(&app->quad_timer);
    knee_flip_reset_heel_counter(&app->heel_counter);

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    app->menu = submenu_alloc();
    app->text_box = text_box_alloc();
    app->block_view = view_alloc();

    if(!app->gui || !app->view_dispatcher || !app->menu || !app->text_box || !app->block_view) {
        return app;
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, knee_flip_navigation_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, knee_flip_tick_callback, 1000);

    view_set_context(app->block_view, app);
    view_allocate_model(app->block_view, ViewModelTypeLocking, sizeof(KneeFlipApp*));
    app->block_model_allocated = true;
    KneeFlipApp** block_model = view_get_model(app->block_view);
    if(block_model) {
        *block_model = app;
        view_commit_model(app->block_view, false);
    }
    view_set_draw_callback(app->block_view, knee_flip_block_draw_callback);
    view_set_input_callback(app->block_view, knee_flip_block_input_callback);

    submenu_add_item(app->menu, "Ice Timer", KneeFlipMenuIceTimer, knee_flip_menu_callback, app);
    submenu_add_item(
        app->menu, "Elevation", KneeFlipMenuElevationTimer, knee_flip_menu_callback, app);
    submenu_add_item(app->menu, "Quad Sets", KneeFlipMenuQuadSets, knee_flip_menu_callback, app);
    submenu_add_item(
        app->menu, "Heel Slides", KneeFlipMenuHeelSlides, knee_flip_menu_callback, app);
    submenu_add_item(app->menu, "About", KneeFlipMenuAbout, knee_flip_menu_callback, app);

    view_dispatcher_add_view(app->view_dispatcher, KneeFlipViewMenu, submenu_get_view(app->menu));
    view_dispatcher_add_view(
        app->view_dispatcher, KneeFlipViewText, text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->view_dispatcher, KneeFlipViewBlock, app->block_view);
    app->views_added = true;

    return app;
}

static void knee_flip_app_free(KneeFlipApp* app) {
    if(!app) {
        return;
    }

    if(app->view_dispatcher) {
        if(app->views_added) {
            view_dispatcher_remove_view(app->view_dispatcher, KneeFlipViewMenu);
            view_dispatcher_remove_view(app->view_dispatcher, KneeFlipViewText);
            view_dispatcher_remove_view(app->view_dispatcher, KneeFlipViewBlock);
        }
        view_dispatcher_free(app->view_dispatcher);
    }

    if(app->block_view) {
        if(app->block_model_allocated) {
            view_free_model(app->block_view);
        }
        view_free(app->block_view);
    }

    if(app->text_box) {
        text_box_free(app->text_box);
    }

    if(app->menu) {
        submenu_free(app->menu);
    }

    if(app->gui) {
        furi_record_close(RECORD_GUI);
    }

    if(app->notification) {
        furi_record_close(RECORD_NOTIFICATION);
    }

    free(app);
}

int32_t knee_flip_rehab_app(void* p) {
    UNUSED(p);

    KneeFlipApp* app = knee_flip_app_alloc();
    if(!app || !app->gui || !app->view_dispatcher || !app->menu || !app->text_box ||
       !app->block_view) {
        knee_flip_app_free(app);
        return -1;
    }

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, KneeFlipViewMenu);
    view_dispatcher_run(app->view_dispatcher);

    knee_flip_app_free(app);
    return 0;
}
