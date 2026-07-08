#include "about_view.h"

#include <furi.h>
#include <string.h>

#define ABOUT_ANIM_PERIOD_MS 45
#define ABOUT_TYPE_SPEED     2
#define ABOUT_BODY_TOP       20
#define ABOUT_LINE_HEIGHT    9

struct AboutView {
    View* view;
    FuriTimer* timer;
};

typedef struct {
    uint32_t anim;
} AboutModel;

// Fake terminal session, revealed one character at a time.
static const char* const about_lines[] = {
    "$ cat about.txt",
    "PocketLab v1.1",
    "One lab at a time.",
    "(c) PerfectoWeb",
    "github.com/PerfectoWeb",
};

static uint32_t about_view_total_chars(void) {
    uint32_t total = 0;
    for(size_t i = 0; i < COUNT_OF(about_lines); i++) {
        total += strlen(about_lines[i]);
    }
    return total;
}

static void about_view_draw_callback(Canvas* canvas, void* context) {
    AboutModel* model = context;
    canvas_clear(canvas);

    // Inverted status bar, like a terminal window title.
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 9, "pocketlab -- about");
    canvas_set_color(canvas, ColorBlack);

    const uint32_t total = about_view_total_chars();
    const uint32_t revealed = model->anim * ABOUT_TYPE_SPEED;
    uint32_t remaining = revealed < total ? revealed : total;

    uint8_t y = ABOUT_BODY_TOP;
    uint8_t cursor_x = 3;
    uint8_t cursor_y = ABOUT_BODY_TOP;

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < COUNT_OF(about_lines); i++) {
        const char* line = about_lines[i];
        const uint8_t length = (uint8_t)strlen(line);
        uint8_t shown = remaining >= length ? length : (uint8_t)remaining;

        char buffer[24];
        if(shown > sizeof(buffer) - 1) {
            shown = sizeof(buffer) - 1;
        }
        memcpy(buffer, line, shown);
        buffer[shown] = '\0';
        canvas_draw_str(canvas, 3, y, buffer);

        cursor_x = 3 + canvas_string_width(canvas, buffer);
        cursor_y = y;
        remaining -= shown;

        if(shown < length) {
            break; // still typing this line
        }
        y += ABOUT_LINE_HEIGHT;
        if(remaining == 0 && i + 1 < COUNT_OF(about_lines)) {
            cursor_x = 3;
            cursor_y = y;
            break;
        }
    }

    const bool typing = revealed < total;
    const bool blink_on = (model->anim / 6) % 2 == 0;
    if(typing || blink_on) {
        canvas_draw_box(canvas, cursor_x + 1, cursor_y - 7, 4, 8);
    }
}

static void about_view_timer_callback(void* context) {
    AboutView* instance = context;
    with_view_model(instance->view, AboutModel * model, { model->anim++; }, true);
}

static void about_view_enter_callback(void* context) {
    AboutView* instance = context;
    with_view_model(instance->view, AboutModel * model, { model->anim = 0; }, true);
    furi_timer_start(instance->timer, furi_ms_to_ticks(ABOUT_ANIM_PERIOD_MS));
}

static void about_view_exit_callback(void* context) {
    AboutView* instance = context;
    furi_timer_stop(instance->timer);
}

AboutView* about_view_alloc(void) {
    AboutView* instance = malloc(sizeof(AboutView));
    instance->view = view_alloc();

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(AboutModel));
    view_set_draw_callback(instance->view, about_view_draw_callback);
    view_set_enter_callback(instance->view, about_view_enter_callback);
    view_set_exit_callback(instance->view, about_view_exit_callback);

    instance->timer = furi_timer_alloc(about_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void about_view_free(AboutView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* about_view_get_view(AboutView* instance) {
    furi_assert(instance);
    return instance->view;
}
