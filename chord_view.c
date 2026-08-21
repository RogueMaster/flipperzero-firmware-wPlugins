#include "chord_view.h"

#include <furi.h>

/* Diagram geometry (screen is 128x64) */
#define GRID_X 34 // x of the low-E string
#define GRID_Y 20 // y of the nut / top fret wire
#define STR_DX 12 // string spacing
#define FRET_DY 8 // fret spacing
#define FRET_ROWS 4 // visible fret spaces
#define GRID_W (STR_DX * (CHORD_STRINGS - 1))
#define GRID_BOTTOM (GRID_Y + FRET_DY * FRET_ROWS)

/* Voicing-cycle arrows: ARROW_H columns wide, (2*ARROW_H - 1) px tall at the back */
#define ARROW_H 4
#define ARROW_Y (GRID_Y + 16) // vertical middle of the grid
#define ARROW_X_L 5 // back edge; apex points left
#define ARROW_X_R 122 // back edge; apex points right

typedef struct {
    Chord chord;
    uint8_t index;
    uint8_t count;
    bool has_chord;
} ChordViewModel;

struct ChordView {
    View* view;
    ChordViewStepCallback step_cb;
    void* context;
    ChordViewStepCallback chord_step_cb;
    void* chord_step_context;
};

static int string_x(size_t s) {
    return GRID_X + (int)s * STR_DX;
}

static int fret_y(int fret) {
    // center of the fret space
    return GRID_Y + (fret - 1) * FRET_DY + FRET_DY / 2;
}

/** Small filled triangle hinting that Left/Right cycles voicings.
 *  `dx` is +1 to point right, -1 to point left; `x` is the flat back edge. */
static void draw_cycle_arrow(Canvas* canvas, int x, int y, int dx) {
    for(int i = 0; i < ARROW_H; i++) {
        int half = (ARROW_H - 1) - i;
        canvas_draw_line(canvas, x + dx * i, y - half, x + dx * i, y + half);
    }
}

static void draw_grid(Canvas* canvas, const Chord* c) {
    canvas_set_color(canvas, ColorBlack);

    for(size_t s = 0; s < CHORD_STRINGS; s++) {
        canvas_draw_line(canvas, string_x(s), GRID_Y, string_x(s), GRID_BOTTOM);
    }
    for(int f = 0; f <= FRET_ROWS; f++) {
        canvas_draw_line(canvas, GRID_X, GRID_Y + f * FRET_DY, GRID_X + GRID_W, GRID_Y + f * FRET_DY);
    }

    if(c->base_fret == 1) {
        // thick nut
        canvas_draw_box(canvas, GRID_X - 1, GRID_Y - 2, GRID_W + 3, 3);
    } else {
        char label[8];
        snprintf(label, sizeof(label), "%ufr", c->base_fret);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, GRID_X - 4, GRID_Y + 5, AlignRight, AlignCenter, label);
    }
}

static void draw_markers(Canvas* canvas, const Chord* c) {
    canvas_set_font(canvas, FontSecondary);
    for(size_t s = 0; s < CHORD_STRINGS; s++) {
        if(c->frets[s] == CHORD_FRET_MUTED) {
            canvas_draw_str_aligned(
                canvas, string_x(s), GRID_Y - 6, AlignCenter, AlignCenter, "x");
        } else if(c->frets[s] == CHORD_FRET_OPEN) {
            canvas_draw_circle(canvas, string_x(s), GRID_Y - 6, 2);
        }
    }
}

/** Draw a barre if one finger covers >= 2 strings on the same fret. */
static void draw_barre(Canvas* canvas, const Chord* c) {
    for(int finger = 1; finger <= 4; finger++) {
        int fret = -1, first = -1, last = -1, n = 0;
        for(size_t s = 0; s < CHORD_STRINGS; s++) {
            if(c->fingers[s] != finger || c->frets[s] <= 0) continue;
            if(fret < 0) fret = c->frets[s];
            if(c->frets[s] != fret) continue;
            if(first < 0) first = (int)s;
            last = (int)s;
            n++;
        }
        if(n >= 2 && fret >= 1 && fret <= FRET_ROWS) {
            int x = string_x(first) - 3;
            int w = string_x(last) - string_x(first) + 7;
            canvas_draw_rbox(canvas, x, fret_y(fret) - 3, w, 7, 3);
        }
    }
}

static void draw_dots(Canvas* canvas, const Chord* c) {
    for(size_t s = 0; s < CHORD_STRINGS; s++) {
        int f = c->frets[s];
        if(f < 1 || f > FRET_ROWS) continue;
        canvas_draw_disc(canvas, string_x(s), fret_y(f), 3);
    }
}

static void draw_fingers(Canvas* canvas, const Chord* c) {
    bool any = false;
    for(size_t s = 0; s < CHORD_STRINGS; s++) {
        if(c->fingers[s] > 0) any = true;
    }
    if(!any) return;

    canvas_set_font(canvas, FontSecondary);
    char buf[2] = {0, 0};
    for(size_t s = 0; s < CHORD_STRINGS; s++) {
        if(c->fingers[s] <= 0) continue;
        buf[0] = (char)('0' + c->fingers[s]);
        canvas_draw_str_aligned(
            canvas, string_x(s), GRID_BOTTOM + 6, AlignCenter, AlignCenter, buf);
    }
}

void chord_diagram_draw(Canvas* canvas, const Chord* chord, bool with_fingers) {
    draw_grid(canvas, chord);
    draw_markers(canvas, chord);
    draw_barre(canvas, chord);
    draw_dots(canvas, chord);
    if(with_fingers) draw_fingers(canvas, chord);
}

static void chord_view_draw(Canvas* canvas, void* model) {
    ChordViewModel* m = model;
    canvas_clear(canvas);

    if(!m->has_chord) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No chord");
        return;
    }

    const Chord* c = &m->chord;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 12, c->name);

    if(m->count > 1) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%u/%u", m->index + 1, m->count);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 125, 8, AlignRight, AlignCenter, buf);
        // triangles: Left/Right cycles the voicings
        draw_cycle_arrow(canvas, ARROW_X_R, ARROW_Y, +1);
        draw_cycle_arrow(canvas, ARROW_X_L, ARROW_Y, -1);
    }

    chord_diagram_draw(canvas, c, true);
}

static bool chord_view_input(InputEvent* event, void* context) {
    ChordView* cv = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyRight) {
        if(cv->step_cb) cv->step_cb(cv->context, +1);
        return true;
    }
    if(event->key == InputKeyLeft) {
        if(cv->step_cb) cv->step_cb(cv->context, -1);
        return true;
    }
    if(event->key == InputKeyDown) {
        if(cv->chord_step_cb) cv->chord_step_cb(cv->chord_step_context, +1);
        return true;
    }
    if(event->key == InputKeyUp) {
        if(cv->chord_step_cb) cv->chord_step_cb(cv->chord_step_context, -1);
        return true;
    }
    return false; // Back falls through to the ViewDispatcher navigation callback
}

ChordView* chord_view_alloc(void) {
    ChordView* cv = malloc(sizeof(ChordView));
    cv->step_cb = NULL;
    cv->context = NULL;
    cv->chord_step_cb = NULL;
    cv->chord_step_context = NULL;
    cv->view = view_alloc();

    view_allocate_model(cv->view, ViewModelTypeLocking, sizeof(ChordViewModel));
    view_set_context(cv->view, cv);
    view_set_draw_callback(cv->view, chord_view_draw);
    view_set_input_callback(cv->view, chord_view_input);
    return cv;
}

void chord_view_free(ChordView* cv) {
    furi_assert(cv);
    view_free(cv->view);
    free(cv);
}

View* chord_view_get_view(ChordView* cv) {
    furi_assert(cv);
    return cv->view;
}

void chord_view_set_step_callback(ChordView* cv, ChordViewStepCallback cb, void* context) {
    furi_assert(cv);
    cv->step_cb = cb;
    cv->context = context;
}

void chord_view_set_chord_step_callback(ChordView* cv, ChordViewStepCallback cb, void* context) {
    furi_assert(cv);
    cv->chord_step_cb = cb;
    cv->chord_step_context = context;
}

void chord_view_set_chord(ChordView* cv, const Chord* chord, uint8_t index, uint8_t count) {
    furi_assert(cv);
    with_view_model(
        cv->view,
        ChordViewModel * m,
        {
            m->chord = *chord;
            m->index = index;
            m->count = count;
            m->has_chord = true;
        },
        true);
}
