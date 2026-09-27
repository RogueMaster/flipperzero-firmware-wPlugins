#include "splash_view.h"
#include "ghosttag_icons.h"
#include <furi.h>

/* 100 ms tick, so 24 frames is 2.4 s end to end. */
#define SPLASH_FRAMES   24
#define SPLASH_NAME_AT  7
#define SPLASH_TAG_AT   13

#define PING_CX 64
#define PING_CY 22

struct SplashView {
    View* view;
    SplashViewCallback done_cb;
    void* done_ctx;
};

typedef struct {
    uint8_t frame;
} SplashModel;

static void splash_view_draw(Canvas* canvas, void* model) {
    SplashModel* m = model;

    /* Three rings leaving the mark, staggered - a ping going out. Each is
     * clipped to the frames where it is actually inside the screen. */
    for(int i = 0; i < 3; i++) {
        int r = (int)m->frame * 3 - i * 9;
        if(r > 2 && r < 34) canvas_draw_circle(canvas, PING_CX, PING_CY, r);
    }

    canvas_draw_icon(canvas, PING_CX - 5, PING_CY - 5, &I_ghost_10px);

    if(m->frame >= SPLASH_NAME_AT) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, "GHOSTTAG");
    }
    if(m->frame >= SPLASH_TAG_AT) {
        canvas_set_font(canvas, FontSecondary);
        /* Baseline 60, not 62: the 'y' and 'g' descenders drop about two rows
         * below the baseline and row 64 does not exist. */
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "hunt the tags that haunt you");
    }
}

static bool splash_view_input(InputEvent* event, void* context) {
    SplashView* sv = context;
    /* Any press skips. Accept Short OR Long - the firmware sends Long instead
     * of Short once a key is held past the long-press threshold, so handling
     * only Short silently drops a firm press. Press/Repeat/Release are ignored
     * so one physical press skips exactly once. */
    if(event->type == InputTypeShort || event->type == InputTypeLong) {
        if(sv->done_cb) sv->done_cb(sv->done_ctx);
        return true;
    }
    /* Swallow the rest so nothing leaks through to the scene manager and
     * unwinds the app out from under the intro. */
    return true;
}

void splash_view_tick(SplashView* sv) {
    furi_assert(sv);
    bool done = false;
    with_view_model(
        sv->view,
        SplashModel * m,
        {
            if(m->frame < SPLASH_FRAMES) {
                m->frame++;
                if(m->frame >= SPLASH_FRAMES) done = true;
            }
        },
        true);
    if(done && sv->done_cb) sv->done_cb(sv->done_ctx);
}

SplashView* splash_view_alloc(void) {
    SplashView* sv = malloc(sizeof(SplashView));
    sv->done_cb = NULL;
    sv->done_ctx = NULL;
    sv->view = view_alloc();
    view_set_context(sv->view, sv);
    view_set_draw_callback(sv->view, splash_view_draw);
    view_set_input_callback(sv->view, splash_view_input);
    view_allocate_model(sv->view, ViewModelTypeLocking, sizeof(SplashModel));
    return sv;
}

void splash_view_free(SplashView* sv) {
    furi_assert(sv);
    view_free(sv->view);
    free(sv);
}

View* splash_view_get_view(SplashView* sv) {
    furi_assert(sv);
    return sv->view;
}

void splash_view_set_done_callback(SplashView* sv, SplashViewCallback cb, void* context) {
    furi_assert(sv);
    sv->done_cb = cb;
    sv->done_ctx = context;
}
