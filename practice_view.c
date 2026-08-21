#include "practice_view.h"
#include "chord_view.h"

#include <furi.h>

#define STEP_DOT_Y 58
#define STEP_DOT_DX 9

typedef struct {
    Chord chord;
    bool has_chord;
    char chord_name[CHORD_NAME_LEN];
    uint8_t step;
    uint8_t count;
    uint16_t bpm;
    bool playing;
} PracticeViewModel;

struct PracticeView {
    View* view;
    PracticeActionCallback action_cb;
    void* context;
};

/** Position within the progression: filled dot = where you are now. */
static void draw_steps(Canvas* canvas, uint8_t step, uint8_t count) {
    if(count == 0) return;
    int x0 = 64 - ((int)count - 1) * STEP_DOT_DX / 2;
    for(uint8_t i = 0; i < count; i++) {
        int x = x0 + (int)i * STEP_DOT_DX;
        if(i == step) {
            canvas_draw_disc(canvas, x, STEP_DOT_Y, 3);
        } else {
            canvas_draw_circle(canvas, x, STEP_DOT_Y, 2);
        }
    }
}

/** Solid play triangle, or two pause bars. */
static void draw_transport(Canvas* canvas, int x, int y, bool playing) {
    if(playing) {
        for(int i = 0; i < 4; i++) {
            canvas_draw_line(canvas, x + i, y - (3 - i), x + i, y + (3 - i));
        }
    } else {
        canvas_draw_box(canvas, x, y - 3, 2, 7);
        canvas_draw_box(canvas, x + 4, y - 3, 2, 7);
    }
}

static void practice_view_draw(Canvas* canvas, void* model) {
    PracticeViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* chord name, same spot as the browse view */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 12, m->chord_name);

    /* tempo + transport, right margin clear of the grid */
    char buf[12];
    snprintf(buf, sizeof(buf), "%ubpm", m->bpm);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 7, AlignRight, AlignCenter, buf);
    draw_transport(canvas, 118, 17, m->playing);

    if(m->has_chord) {
        chord_diagram_draw(canvas, &m->chord, false);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "not in library");
    }

    draw_steps(canvas, m->step, m->count);
}

static bool practice_view_input(InputEvent* event, void* context) {
    PracticeView* pv = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(!pv->action_cb) return false;

    switch(event->key) {
    case InputKeyRight:
        pv->action_cb(pv->context, PracticeActionNext);
        return true;
    case InputKeyLeft:
        pv->action_cb(pv->context, PracticeActionPrev);
        return true;
    case InputKeyUp:
        pv->action_cb(pv->context, PracticeActionFaster);
        return true;
    case InputKeyDown:
        pv->action_cb(pv->context, PracticeActionSlower);
        return true;
    case InputKeyOk:
        pv->action_cb(pv->context, PracticeActionToggle);
        return true;
    default:
        return false; // Back falls through to the ViewDispatcher
    }
}

PracticeView* practice_view_alloc(void) {
    PracticeView* pv = malloc(sizeof(PracticeView));
    pv->action_cb = NULL;
    pv->context = NULL;
    pv->view = view_alloc();

    view_allocate_model(pv->view, ViewModelTypeLocking, sizeof(PracticeViewModel));
    view_set_context(pv->view, pv);
    view_set_draw_callback(pv->view, practice_view_draw);
    view_set_input_callback(pv->view, practice_view_input);
    return pv;
}

void practice_view_free(PracticeView* pv) {
    furi_assert(pv);
    view_free(pv->view);
    free(pv);
}

View* practice_view_get_view(PracticeView* pv) {
    furi_assert(pv);
    return pv->view;
}

void practice_view_set_action_callback(PracticeView* pv, PracticeActionCallback cb, void* context) {
    furi_assert(pv);
    pv->action_cb = cb;
    pv->context = context;
}

void practice_view_update(
    PracticeView* pv,
    const Chord* chord,
    const char* chord_name,
    uint8_t step,
    uint8_t count,
    uint16_t bpm,
    bool playing) {
    furi_assert(pv);
    with_view_model(
        pv->view,
        PracticeViewModel * m,
        {
            m->has_chord = (chord != NULL);
            if(chord) m->chord = *chord;
            strncpy(m->chord_name, chord_name, CHORD_NAME_LEN - 1);
            m->chord_name[CHORD_NAME_LEN - 1] = '\0';
            m->step = step;
            m->count = count;
            m->bpm = bpm;
            m->playing = playing;
        },
        true);
}
