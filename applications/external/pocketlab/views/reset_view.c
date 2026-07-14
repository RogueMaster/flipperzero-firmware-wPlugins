#include "reset_view.h"

#include <furi.h>
#include <input/input.h>
#include <string.h>

#include "../helpers/pocketlab_fonts.h"
#include "../helpers/pocketlab_i18n.h"

// Custom confirmation page used for Russian/Spanish, drawn with the app font so
// the text renders. English keeps the stock Widget dialog. Layout mirrors it:
// a title, a two-line note, and Cancel (left) / Reset (right) buttons.

struct ResetView {
    View* view;
    ResetViewCallback callback;
    void* context;
};

typedef struct {
    uint8_t dummy;
} ResetModel;

// Greedy word-wrap centred on cx using the current canvas font.
static void reset_draw_wrapped(Canvas* canvas, uint8_t cx, uint8_t y, uint8_t w, uint8_t line_h) {
    const char* text = pocketlab_text(PocketLabTextResetBody);
    char line[64] = {0};
    uint8_t cy = y;
    const char* p = text;
    char word[48];
    while(*p) {
        while(*p == ' ')
            p++;
        size_t wi = 0;
        while(*p && *p != ' ' && wi < sizeof(word) - 1)
            word[wi++] = *p++;
        word[wi] = '\0';
        if(!wi) break;

        char trial[112];
        if(line[0]) {
            snprintf(trial, sizeof(trial), "%s %s", line, word);
        } else {
            snprintf(trial, sizeof(trial), "%s", word);
        }
        if(canvas_string_width(canvas, trial) <= w) {
            strncpy(line, trial, sizeof(line) - 1);
        } else {
            if(line[0]) {
                canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignTop, line);
                cy += line_h;
            }
            strncpy(line, word, sizeof(line) - 1);
        }
    }
    if(line[0]) canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignTop, line);
}

static void reset_view_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);

    pocketlab_font_apply(canvas, true);
    canvas_draw_str_aligned(
        canvas, 64, 12, AlignCenter, AlignTop, pocketlab_text(PocketLabTextResetTitle));

    pocketlab_font_apply_small(canvas);
    reset_draw_wrapped(canvas, 64, 28, 120, 11);

    // Cancel button in the bottom-left, Reset in the bottom-right.
    const char* cancel = pocketlab_text(PocketLabTextCancel);
    const char* reset = pocketlab_text(PocketLabTextResetBtn);
    const uint8_t lw = (uint8_t)(canvas_string_width(canvas, cancel) + 10);
    const uint8_t rw = (uint8_t)(canvas_string_width(canvas, reset) + 10);

    canvas_draw_rbox(canvas, 0, 51, lw, 13, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, lw / 2, 58, AlignCenter, AlignCenter, cancel);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rbox(canvas, (uint8_t)(128 - rw), 51, rw, 13, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, (uint8_t)(128 - rw / 2), 58, AlignCenter, AlignCenter, reset);
    canvas_set_color(canvas, ColorBlack);
}

static bool reset_view_input_callback(InputEvent* event, void* context) {
    ResetView* instance = context;
    if(event->type != InputTypeShort) {
        return false;
    }
    if(event->key == InputKeyLeft) {
        if(instance->callback) instance->callback(instance->context, false);
        return true;
    }
    if(event->key == InputKeyRight) {
        if(instance->callback) instance->callback(instance->context, true);
        return true;
    }
    return false; // let Back propagate to the scene navigation (cancel)
}

ResetView* reset_view_alloc(void) {
    ResetView* instance = malloc(sizeof(ResetView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(ResetModel));
    view_set_draw_callback(instance->view, reset_view_draw_callback);
    view_set_input_callback(instance->view, reset_view_input_callback);

    return instance;
}

void reset_view_free(ResetView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* reset_view_get_view(ResetView* instance) {
    furi_assert(instance);
    return instance->view;
}

void reset_view_set_callback(ResetView* instance, ResetViewCallback callback, void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}
