#include "splash_view.h"

#include <furi.h>
#include <gui/gui.h>

/* Pouch geometry (centred, top of screen). */
#define PCX     64 // pouch centre x
#define PCY     18 // pouch centre y
#define POUCH_X 40
#define POUCH_Y 4
#define POUCH_W 48
#define POUCH_H 28

struct SplashView {
    View* view;
    SplashViewSkipCallback skip_cb;
    void* skip_ctx;
};

typedef struct {
    uint8_t anim;
    uint8_t progress; // 0..100
} SplashModel;

static void splash_view_draw(Canvas* canvas, void* model) {
    SplashModel* m = model;

    /* --- radiating waves, contained by the pouch --- */
    /* Three rings expanding from the fob; phased by the animation clock so they
     * appear to emanate. Kept small enough to sit inside the pouch, which is
     * then framed on top so the waves read as "pressed against the shield". */
    for(int i = 0; i < 3; i++) {
        int r = 3 + ((m->anim + i * 4) % 12);
        if(r > 1) canvas_draw_circle(canvas, PCX, PCY, r);
    }

    /* the fob at the centre */
    canvas_draw_rbox(canvas, PCX - 4, PCY - 5, 8, 10, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, PCX, PCY - 1);
    canvas_set_color(canvas, ColorBlack);

    /* the pouch shell on top, containing the waves */
    canvas_draw_rframe(canvas, POUCH_X, POUCH_Y, POUCH_W, POUCH_H, 4);
    canvas_draw_rframe(canvas, POUCH_X + 1, POUCH_Y + 1, POUCH_W - 2, POUCH_H - 2, 3);
    /* seal teeth along the top */
    for(int tx = POUCH_X + 6; tx < POUCH_X + POUCH_W - 4; tx += 6) {
        canvas_draw_line(canvas, tx, POUCH_Y + 2, tx, POUCH_Y + 5);
    }

    /* the faint leak escaping the right seam - blinks */
    if((m->anim % 6) < 3) {
        canvas_draw_dot(canvas, POUCH_X + POUCH_W + 2, PCY);
        canvas_draw_dot(canvas, POUCH_X + POUCH_W + 5, PCY);
    }

    /* --- wordmark + tagline --- */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, "FARADAY");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignBottom, "prove your pouch works");

    /* --- auto-advance progress bar --- */
    canvas_draw_frame(canvas, 24, 58, 80, 4);
    int fw = (76 * (m->progress > 100 ? 100 : m->progress)) / 100;
    if(fw > 0) canvas_draw_box(canvas, 26, 60, fw, 1);
}

static bool splash_view_input(InputEvent* event, void* context) {
    SplashView* v = context;
    /* Any short press skips straight to the menu; Back falls through so it
     * exits the app as usual. */
    if(event->type == InputTypeShort && event->key != InputKeyBack) {
        if(v->skip_cb) v->skip_cb(v->skip_ctx);
        return true;
    }
    return false;
}

SplashView* splash_view_alloc(void) {
    SplashView* v = malloc(sizeof(SplashView));
    v->skip_cb = NULL;
    v->skip_ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, splash_view_draw);
    view_set_input_callback(v->view, splash_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SplashModel));
    return v;
}

void splash_view_free(SplashView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* splash_view_get_view(SplashView* v) {
    furi_assert(v);
    return v->view;
}

void splash_view_set_skip_callback(SplashView* v, SplashViewSkipCallback cb, void* context) {
    furi_assert(v);
    v->skip_cb = cb;
    v->skip_ctx = context;
}

void splash_view_set_progress(SplashView* v, uint8_t progress) {
    furi_assert(v);
    with_view_model(v->view, SplashModel * m, { m->progress = progress; }, true);
}

void splash_view_tick(SplashView* v) {
    furi_assert(v);
    with_view_model(v->view, SplashModel * m, { m->anim++; }, true);
}
