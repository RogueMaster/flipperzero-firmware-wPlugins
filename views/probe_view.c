#include "probe_view.h"
#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

/* Two pages, Left/Right to flip:
 *   1) how to wire the probe, as a schematic plus the pin mapping
 *   2) a live check, so you can prove the thing works before you trust a sweep
 *
 * Page 2 matters more than it looks. A probe that is wired wrong reads a flat
 * zero, which is indistinguishable from "this room is clean" — so Nyx gives you
 * a way to make the needle move on demand with a TV remote.
 */

#define PROBE_PAGES 2
#define BAR_FULL_MV 2500 // matches the ADC's 2.5 V scale

struct ProbeView {
    View* view;
};

typedef struct {
    uint8_t page;
    char pin_name[8];
    uint8_t pin_number;
    bool detected;
    uint16_t mv;
    uint16_t peak_mv;
    uint8_t anim;
} ProbeModel;

static void draw_header(Canvas* canvas, const ProbeModel* m) {
    char buf[8];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, m->page == 0 ? "PROBE WIRING" : "PROBE CHECK");
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)(m->page + 1), (unsigned)PROBE_PAGES);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 11, 127, 11);
}

/* A small arrowhead at (x,y), pointing down-right — the "light falls on me"
 * marker that turns a plain transistor symbol into a photo- one. */
static void draw_light_arrow(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x - 7, y - 7, x, y);
    canvas_draw_line(canvas, x, y, x - 4, y);
    canvas_draw_line(canvas, x, y, x, y - 4);
}

static void draw_wiring(Canvas* canvas, const ProbeModel* m) {
    char buf[16];

    /* ---- schematic, left ---- */
    /* 3V3 rail */
    canvas_draw_line(canvas, 14, 17, 26, 17);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 29, 20, "3V3");

    /* phototransistor: body, junction, and light falling in from the left */
    canvas_draw_line(canvas, 20, 17, 20, 21);
    canvas_draw_circle(canvas, 20, 27, 6);
    canvas_draw_line(canvas, 16, 31, 24, 23);
    draw_light_arrow(canvas, 12, 24);
    draw_light_arrow(canvas, 12, 32);

    /* rail down to the tap */
    canvas_draw_line(canvas, 20, 33, 20, 42);

    /* tap node out to the ADC pin */
    canvas_draw_disc(canvas, 20, 38, 1);
    canvas_draw_line(canvas, 20, 38, 42, 38);
    canvas_draw_str(canvas, 44, 41, "ADC");

    /* load resistor */
    canvas_draw_frame(canvas, 17, 42, 7, 11);
    canvas_draw_str(canvas, 27, 50, "10k");

    /* rail down to ground */
    canvas_draw_line(canvas, 20, 53, 20, 57);
    canvas_draw_line(canvas, 14, 57, 26, 57);
    canvas_draw_line(canvas, 16, 59, 24, 59);
    canvas_draw_line(canvas, 18, 61, 22, 61);

    /* ---- pin mapping, right ---- */
    canvas_draw_line(canvas, 60, 12, 60, 63);
    canvas_draw_str(canvas, 64, 20, "PHOTO-TR");
    canvas_draw_str(canvas, 64, 31, "C  3V3   p9");
    snprintf(buf, sizeof(buf), "E  %.4s  p%u", m->pin_name, (unsigned)m->pin_number);
    canvas_draw_str(canvas, 64, 41, buf);
    canvas_draw_str(canvas, 64, 51, "E  10k to");
    canvas_draw_str(canvas, 64, 61, "   GND  p18");
}

static void draw_check(Canvas* canvas, const ProbeModel* m) {
    char buf[24];

    /* ---- detected badge ---- */
    if(m->detected) {
        canvas_draw_box(canvas, 6, 14, 116, 13);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, "PROBE DETECTED");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_frame(canvas, 6, 14, 116, 13);
        canvas_set_font(canvas, FontPrimary);
        snprintf(buf, sizeof(buf), "NOTHING ON %.4s", m->pin_name);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, buf);
    }

    /* ---- live reading ---- */
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%u", (unsigned)m->mv);
    canvas_draw_str_aligned(canvas, 74, 45, AlignRight, AlignBottom, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 77, 44, "mV");

    snprintf(buf, sizeof(buf), "pk %u", (unsigned)m->peak_mv);
    canvas_draw_str_aligned(canvas, 126, 44, AlignRight, AlignBottom, buf);

    /* ---- bar with peak tick ---- */
    canvas_draw_frame(canvas, 2, 47, 124, 7);
    uint32_t w = ((uint32_t)m->mv * 122u) / BAR_FULL_MV;
    if(w > 122u) w = 122u;
    if(w > 0) canvas_draw_box(canvas, 3, 48, w, 5);
    uint32_t pk = ((uint32_t)m->peak_mv * 122u) / BAR_FULL_MV;
    if(pk > 121u) pk = 121u;
    canvas_draw_line(canvas, 3 + pk, 46, 3 + pk, 54);

    /* ---- hint ---- */
    const char* hint = ((m->anim / 30u) % 2u) ? "Reading should jump" :
                                                "Aim a TV remote, press a key";
    canvas_draw_str(canvas, 2, 62, hint);
}

static void probe_view_draw(Canvas* canvas, void* model) {
    ProbeModel* m = model;
    draw_header(canvas, m);
    if(m->page == 0) {
        draw_wiring(canvas, m);
    } else {
        draw_check(canvas, m);
    }
}

static bool probe_view_input(InputEvent* event, void* context) {
    ProbeView* v = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyRight) {
        with_view_model(
            v->view,
            ProbeModel * m,
            {
                if(m->page + 1 < PROBE_PAGES) m->page++;
                m->peak_mv = 0; // each visit to the check page starts fresh
            },
            true);
        return true;
    }
    if(event->key == InputKeyLeft) {
        bool consumed = false;
        with_view_model(
            v->view,
            ProbeModel * m,
            {
                if(m->page > 0) {
                    m->page--;
                    consumed = true;
                }
            },
            true);
        return consumed; // Left on the first page falls through to Back
    }
    return false;
}

ProbeView* probe_view_alloc(void) {
    ProbeView* v = malloc(sizeof(ProbeView));
    memset(v, 0, sizeof(ProbeView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, probe_view_draw);
    view_set_input_callback(v->view, probe_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ProbeModel));
    return v;
}

void probe_view_free(ProbeView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* probe_view_get_view(ProbeView* v) {
    furi_assert(v);
    return v->view;
}

void probe_view_update(
    ProbeView* v,
    const char* pin_name,
    uint8_t pin_number,
    bool detected,
    uint16_t mv) {
    furi_assert(v);
    with_view_model(
        v->view,
        ProbeModel * m,
        {
            if(pin_name) {
                strncpy(m->pin_name, pin_name, sizeof(m->pin_name) - 1);
                m->pin_name[sizeof(m->pin_name) - 1] = '\0';
            }
            m->pin_number = pin_number;
            m->detected = detected;
            m->mv = mv;
            if(mv > m->peak_mv) m->peak_mv = mv;
        },
        true);
}

void probe_view_tick(ProbeView* v) {
    furi_assert(v);
    with_view_model(v->view, ProbeModel * m, { m->anim++; }, true);
}
