/* About screen — pixel-perfect layout for the 128x64 monochrome canvas.
 *
 *   y =  0 .. 11   bold project name centred ("wmbuster")
 *   y = 14 .. 22   description line 1, FontSecondary
 *   y = 24 .. 32   description line 2, FontSecondary
 *   y = 36         single-pixel divider across the screen
 *   y = 40 .. 51   URL line 1 ("github.com/i12bp8") in FontPrimary, centred
 *   y = 52 .. 63   URL line 2 ("/wmbuster")          in FontPrimary, centred
 *
 * The two-line URL is required because FontPrimary is wide enough that the
 * full path overflows the screen on a single line. Both lines are bold to
 * keep the link visually unified. */

#include "about_canvas.h"

#include <gui/canvas.h>
#include <gui/view.h>
#include <input/input.h>
#include <stdlib.h>

struct AboutCanvas {
    View* view;
};

static void about_draw(Canvas* c, void* model) {
    (void)model;
    canvas_clear(c);

    /* Title: bold, centred. */
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, "wmbuster");

    /* Description: two short, secondary-font lines. */
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 14, AlignCenter, AlignTop,
                            "EU wM-Bus listener");
    canvas_draw_str_aligned(c, 64, 24, AlignCenter, AlignTop,
                            "T1 . C1 . S1 . AES-128");

    /* Divider. */
    canvas_draw_line(c, 8, 36, 119, 36);

    /* GitHub link, bold, two-line. */
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 40, AlignCenter, AlignTop,
                            "github.com/i12bp8");
    canvas_draw_str_aligned(c, 64, 52, AlignCenter, AlignTop,
                            "/wmbuster");
}

static bool about_input(InputEvent* e, void* ctx) {
    (void)ctx;
    /* Only consume Back so the scene manager can pop us; let everything else
     * fall through to the default no-op. */
    if(e->type == InputTypeShort && e->key == InputKeyBack) {
        return false;  /* let dispatcher handle navigation */
    }
    return false;
}

AboutCanvas* about_canvas_alloc(void) {
    AboutCanvas* a = malloc(sizeof(*a));
    a->view = view_alloc();
    view_set_context(a->view, a);
    view_set_draw_callback(a->view, about_draw);
    view_set_input_callback(a->view, about_input);
    return a;
}

void about_canvas_free(AboutCanvas* a) {
    if(!a) return;
    view_free(a->view);
    free(a);
}

View* about_canvas_get_view(AboutCanvas* a) { return a->view; }
