#include "htw_setup_view.h"
#include <furi.h>
#include <gui/elements.h>
#include <stdlib.h>

// Screen dimensions for vertical mode
#define SCREEN_WIDTH  64
#define SCREEN_HEIGHT 128

// Model
typedef struct {
    HtwState* state;
} HtwSetupViewModel;

struct HtwSetupView {
    View* view;
};

// Helper: Draw toggle switch
static void draw_toggle_switch(Canvas* canvas, int16_t x, int16_t y, bool value, bool focused) {
    int16_t sw_w = 36;
    int16_t sw_h = 16;

    // Draw switch background
    if(focused) {
        canvas_draw_rbox(canvas, x, y, sw_w, sw_h, sw_h / 2);
        canvas_set_color(canvas, ColorWhite);

        // Draw inner track
        canvas_draw_rframe(canvas, x + 2, y + 2, sw_w - 4, sw_h - 4, (sw_h - 4) / 2);

        // Draw knob position
        if(value) {
            // ON - knob on right
            canvas_draw_disc(canvas, x + sw_w - sw_h / 2 - 2, y + sw_h / 2, 4);
        } else {
            // OFF - knob on left
            canvas_draw_disc(canvas, x + sw_h / 2 + 2, y + sw_h / 2, 4);
        }

        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, x, y, sw_w, sw_h, sw_h / 2);

        // Draw knob
        if(value) {
            canvas_draw_disc(canvas, x + sw_w - sw_h / 2 - 2, y + sw_h / 2, 5);
        } else {
            canvas_draw_circle(canvas, x + sw_h / 2 + 2, y + sw_h / 2, 5);
        }
    }
}

static void htw_setup_view_draw(Canvas* canvas, void* model) {
    HtwSetupViewModel* m = model;
    if(!m->state) return;

    canvas_clear(canvas);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 2, AlignCenter, AlignTop, "Settings");

    // Separator line
    canvas_draw_line(canvas, 0, 14, SCREEN_WIDTH, 14);

    // Save state option
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, 22, AlignCenter, AlignTop, "Save state");

    // ON/OFF label
    canvas_set_font(canvas, FontPrimary);
    const char* label = m->state->save_state ? "ON" : "OFF";
    canvas_draw_str_aligned(canvas, 32, 36, AlignCenter, AlignTop, label);

    // Toggle switch (always focused since it's the only control)
    draw_toggle_switch(canvas, 14, 52, m->state->save_state, true);

    // Description box
    canvas_draw_rframe(canvas, 2, 76, 60, 46, 3);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, 80, AlignCenter, AlignTop, "When ON:");
    canvas_draw_str_aligned(canvas, 32, 92, AlignCenter, AlignTop, "Settings are");
    canvas_draw_str_aligned(canvas, 32, 104, AlignCenter, AlignTop, "saved to SD");
}

static bool htw_setup_view_input(InputEvent* event, void* context) {
    HtwSetupView* view = context;
    bool consumed = false;

    with_view_model(
        view->view,
        HtwSetupViewModel * m,
        {
            if(event->type == InputTypeShort) {
                switch(event->key) {
                case InputKeyLeft:
                case InputKeyRight:
                case InputKeyOk:
                    // Toggle save_state
                    m->state->save_state = !m->state->save_state;
                    consumed = true;
                    break;
                default:
                    break;
                }
            }
        },
        consumed);

    return consumed;
}

HtwSetupView* htw_setup_view_alloc(void) {
    HtwSetupView* view = malloc(sizeof(HtwSetupView));
    view->view = view_alloc();

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(HtwSetupViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, htw_setup_view_draw);
    view_set_input_callback(view->view, htw_setup_view_input);
    view_set_orientation(view->view, ViewOrientationVertical);

    with_view_model(view->view, HtwSetupViewModel * m, { m->state = NULL; }, true);

    return view;
}

void htw_setup_view_free(HtwSetupView* view) {
    if(view) {
        view_free(view->view);
        free(view);
    }
}

View* htw_setup_view_get_view(HtwSetupView* view) {
    return view->view;
}

void htw_setup_view_set_state(HtwSetupView* view, HtwState* state) {
    with_view_model(view->view, HtwSetupViewModel * m, { m->state = state; }, true);
}
