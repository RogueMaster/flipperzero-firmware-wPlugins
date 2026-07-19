#include "wiring_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdio.h>

struct WiringView {
    View* view;
};

typedef struct {
    uint8_t out_header;
    uint8_t in_header;
    char out_name[6];
    char in_name[6];
    bool show_trigger_in;
    uint8_t tip; // index, advanced slowly by tick
    uint16_t tick;
} WiringModel;

/* Two-line safety tips, sized to fit the bottom band. */
static const char* const tips[] = {
    "3V3 logic only - never\nwire a rail to a GPIO",
    "Switch VCC with a MOSFET\ncrowbar, not the pin",
    "Share a common ground\nFlipper <-> target",
    "Series gate resistor,\nkeep the leads short",
    "Only glitch boards you\nown & may lawfully test",
};
#define TIP_COUNT (sizeof(tips) / sizeof(tips[0]))

static void draw_box(Canvas* canvas, int x, int y, int w, int h, const char* title) {
    canvas_draw_rframe(canvas, x, y, w, h, 2);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + w / 2, y - 1, AlignCenter, AlignBottom, title);
}

static void wiring_view_draw(Canvas* canvas, void* model) {
    WiringModel* m = model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "Wiring");
    canvas_draw_line(canvas, 0, 11, 128, 11);

    /* --- three blocks: Flipper -> MOSFET -> Target --- */
    const int by = 22, bh = 22;
    draw_box(canvas, 2, by, 40, bh, "FLIPPER");
    draw_box(canvas, 55, by, 26, bh, "SWITCH");
    draw_box(canvas, 94, by, 32, bh, "TARGET");

    canvas_set_font(canvas, FontSecondary);
    char line[16];

    /* Flipper pins */
    snprintf(line, sizeof(line), "P%u glt", m->out_header);
    canvas_draw_str(canvas, 5, by + 9, line);
    if(m->show_trigger_in) {
        snprintf(line, sizeof(line), "P%u trg", m->in_header);
        canvas_draw_str(canvas, 5, by + 17, line);
    } else {
        canvas_draw_str(canvas, 5, by + 17, "P8 gnd");
    }

    /* Switch block: a tiny NMOS glyph */
    int gx = 62, gy = by + 11;
    canvas_draw_line(canvas, 55, gy, gx, gy); // gate lead in
    canvas_draw_line(canvas, gx, by + 4, gx, by + bh - 4); // gate bar
    canvas_draw_line(canvas, gx + 2, by + 5, gx + 2, by + bh - 5); // channel
    canvas_draw_line(canvas, gx + 2, by + 6, 78, by + 6); // drain up-ish
    canvas_draw_line(canvas, gx + 2, by + bh - 6, 78, by + bh - 6); // source

    /* Target */
    canvas_draw_str(canvas, 97, by + 9, "VCC");
    canvas_draw_str(canvas, 97, by + 17, "GND");

    /* connections */
    canvas_draw_line(canvas, 42, by + 9, 55, by + 9); // Flipper glt -> gate region
    canvas_draw_line(canvas, 78, by + 6, 94, by + 6); // drain -> target VCC (switched)
    /* common ground rail */
    canvas_draw_line(canvas, 12, by + bh + 3, 116, by + bh + 3);
    canvas_draw_line(canvas, 12, by + bh, 12, by + bh + 3);
    canvas_draw_line(canvas, 78, by + bh - 6, 78, by + bh + 3);
    canvas_draw_line(canvas, 110, by + bh, 110, by + bh + 3);
    canvas_draw_str(canvas, 52, by + bh + 2, "GND");

    /* rotating safety tip */
    elements_multiline_text_aligned(canvas, 64, 55, AlignCenter, AlignTop, tips[m->tip % TIP_COUNT]);
}

static bool wiring_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; // Back leaves the scene
}

WiringView* wiring_view_alloc(void) {
    WiringView* v = malloc(sizeof(WiringView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(WiringModel));
    view_set_draw_callback(v->view, wiring_view_draw);
    view_set_input_callback(v->view, wiring_view_input);
    with_view_model(
        v->view,
        WiringModel * m,
        {
            strncpy(m->out_name, "PA7", sizeof(m->out_name) - 1);
            strncpy(m->in_name, "PB2", sizeof(m->in_name) - 1);
            m->out_header = 2;
            m->in_header = 6;
            m->show_trigger_in = true;
        },
        false);
    return v;
}

void wiring_view_free(WiringView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* wiring_view_get_view(WiringView* v) {
    furi_assert(v);
    return v->view;
}

void wiring_view_set_pins(
    WiringView* v,
    uint8_t out_header,
    const char* out_name,
    uint8_t in_header,
    const char* in_name,
    bool show_trigger_in) {
    with_view_model(
        v->view,
        WiringModel * m,
        {
            m->out_header = out_header;
            m->in_header = in_header;
            strncpy(m->out_name, out_name, sizeof(m->out_name) - 1);
            m->out_name[sizeof(m->out_name) - 1] = 0;
            strncpy(m->in_name, in_name, sizeof(m->in_name) - 1);
            m->in_name[sizeof(m->in_name) - 1] = 0;
            m->show_trigger_in = show_trigger_in;
        },
        true);
}

void wiring_view_tick(WiringView* v) {
    with_view_model(
        v->view,
        WiringModel * m,
        {
            m->tick++;
            if(m->tick % 60 == 0) m->tip++; // ~2 s per tip at 32 ms ticks
        },
        true);
}
