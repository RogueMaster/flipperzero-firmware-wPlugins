#include "wiring_view.h"

#include <gui/elements.h>
#include <string.h>

#define WIRING_PAGES (2u)

/* Wire geometry, shared by the drawing and the travelling dots.
 *
 *   0..10   title + separator
 *   18      the FLIPPER / TARGET captions sit on their own baseline, clear of
 *           the separator - they used to overlap it
 *   20..56  the two boxes, with the wires strung between them
 *   63      footer
 */
#define WIRE_BOX_Y (20)
#define WIRE_BOX_H (36)
#define WIRE_CAPTION_BASELINE (18)

#define WIRE_X_LEFT (28) // right edge of the Flipper box
#define WIRE_X_RIGHT (100) // left edge of the target box
#define WIRE_Y_RX (28)
#define WIRE_Y_TX (38)
#define WIRE_Y_GND (48)

#define WIRE_LABEL_L (26) // pin numbers, right-aligned inside the Flipper box
#define WIRE_LABEL_R (102) // signal names, left-aligned inside the target box
#define WIRE_TEXT_DROP (3) // baseline offset that centres text on its wire

typedef struct {
    uint8_t page;
    uint8_t rx_pin;
    uint8_t tx_pin;
} WiringViewModel;

struct WiringView {
    View* view;
};

/* --------------------------------------------------------------- page 0 --- */

/** Dots crawling from the target's TX toward the Flipper's RX - the direction
 *  the data actually travels, which is the whole point of the cross-over. */
static void wiring_draw_flow(Canvas* canvas) {
    const uint32_t phase = (furi_get_tick() / 40u) % 12u;
    for(int x = WIRE_X_RIGHT - (int)phase; x > WIRE_X_LEFT; x -= 12) {
        if(x <= WIRE_X_LEFT + 1) continue;
        canvas_draw_box(canvas, x - 1, WIRE_Y_RX - 1, 3, 3);
    }
}

static void wiring_draw_diagram(Canvas* canvas, const WiringViewModel* m) {
    char buf[8];

    canvas_set_font(canvas, FontSecondary);

    /* the two boxes, captioned above rather than through the separator */
    elements_slightly_rounded_frame(canvas, 2, WIRE_BOX_Y, 26, WIRE_BOX_H);
    elements_slightly_rounded_frame(canvas, WIRE_X_RIGHT, WIRE_BOX_Y, 26, WIRE_BOX_H);
    canvas_draw_str(canvas, 2, WIRE_CAPTION_BASELINE, "FLIPPER");
    canvas_draw_str_aligned(canvas, 126, WIRE_CAPTION_BASELINE, AlignRight, AlignBottom, "TARGET");

    /* RX <- TX, the one that matters */
    canvas_draw_line(canvas, WIRE_X_LEFT, WIRE_Y_RX, WIRE_X_RIGHT, WIRE_Y_RX);
    wiring_draw_flow(canvas);
    snprintf(buf, sizeof(buf), "%u", m->rx_pin);
    canvas_draw_str_aligned(
        canvas, WIRE_LABEL_L, WIRE_Y_RX + WIRE_TEXT_DROP, AlignRight, AlignBottom, buf);
    canvas_draw_str(canvas, WIRE_LABEL_R, WIRE_Y_RX + WIRE_TEXT_DROP, "TX");

    /* TX -> RX, dashed: optional until you actually type */
    for(int x = WIRE_X_LEFT; x < WIRE_X_RIGHT; x += 4) {
        canvas_draw_line(canvas, x, WIRE_Y_TX, x + 1, WIRE_Y_TX);
    }
    snprintf(buf, sizeof(buf), "%u", m->tx_pin);
    canvas_draw_str_aligned(
        canvas, WIRE_LABEL_L, WIRE_Y_TX + WIRE_TEXT_DROP, AlignRight, AlignBottom, buf);
    canvas_draw_str(canvas, WIRE_LABEL_R, WIRE_Y_TX + WIRE_TEXT_DROP, "RX");

    /* GND, drawn heavy because forgetting it is the number one failure */
    canvas_draw_line(canvas, WIRE_X_LEFT, WIRE_Y_GND, WIRE_X_RIGHT, WIRE_Y_GND);
    canvas_draw_line(canvas, WIRE_X_LEFT, WIRE_Y_GND + 1, WIRE_X_RIGHT, WIRE_Y_GND + 1);
    canvas_draw_str_aligned(
        canvas, WIRE_LABEL_L, WIRE_Y_GND + WIRE_TEXT_DROP, AlignRight, AlignBottom, "8");
    canvas_draw_str(canvas, WIRE_LABEL_R, WIRE_Y_GND + WIRE_TEXT_DROP, "GND");

    /* pin pads */
    canvas_draw_disc(canvas, WIRE_X_LEFT, WIRE_Y_RX, 1);
    canvas_draw_disc(canvas, WIRE_X_LEFT, WIRE_Y_TX, 1);
    canvas_draw_disc(canvas, WIRE_X_LEFT, WIRE_Y_GND, 1);
    canvas_draw_disc(canvas, WIRE_X_RIGHT, WIRE_Y_RX, 1);
    canvas_draw_disc(canvas, WIRE_X_RIGHT, WIRE_Y_TX, 1);
    canvas_draw_disc(canvas, WIRE_X_RIGHT, WIRE_Y_GND, 1);

    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "RX and TX always cross");
}

/* --------------------------------------------------------------- page 1 --- */

static void wiring_draw_rules(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 21, "3.3V logic only");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 32, "RS-232 (+/-12V) kills the");
    canvas_draw_str(canvas, 2, 41, "Flipper. Use a MAX3232.");
    canvas_draw_str(canvas, 2, 53, "Ground first. Never wire 5V");
    canvas_draw_str(canvas, 2, 62, "into a self-powered board.");
}

/* ---------------------------------------------------------------- draw ---- */

static void wiring_view_draw(Canvas* canvas, void* model) {
    const WiringViewModel* m = model;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 8, (m->page == 0) ? "Wiring" : "Rules");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, (m->page == 0) ? "1/2" : "2/2");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    if(m->page == 0) {
        wiring_draw_diagram(canvas, m);
    } else {
        wiring_draw_rules(canvas);
    }
}

static bool wiring_view_input(InputEvent* event, void* context) {
    WiringView* wv = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyRight) {
        with_view_model(
            wv->view,
            WiringViewModel * m,
            {
                if((uint32_t)m->page + 1u < WIRING_PAGES) m->page++;
            },
            true);
        return true;
    }
    if(event->key == InputKeyLeft) {
        with_view_model(
            wv->view,
            WiringViewModel * m,
            {
                if(m->page > 0) m->page--;
            },
            true);
        return true;
    }
    return false;
}

/* ----------------------------------------------------------------- api ---- */

WiringView* wiring_view_alloc(void) {
    WiringView* wv = malloc(sizeof(WiringView));
    wv->view = view_alloc();
    view_allocate_model(wv->view, ViewModelTypeLocking, sizeof(WiringViewModel));
    view_set_context(wv->view, wv);
    view_set_draw_callback(wv->view, wiring_view_draw);
    view_set_input_callback(wv->view, wiring_view_input);
    with_view_model(
        wv->view,
        WiringViewModel * m,
        {
            m->page = 0;
            m->rx_pin = 14;
            m->tx_pin = 13;
        },
        true);
    return wv;
}

void wiring_view_free(WiringView* wv) {
    furi_assert(wv);
    view_free(wv->view);
    free(wv);
}

View* wiring_view_get_view(WiringView* wv) {
    furi_assert(wv);
    return wv->view;
}

void wiring_view_set_pins(WiringView* wv, uint8_t rx_pin, uint8_t tx_pin) {
    furi_assert(wv);
    with_view_model(
        wv->view,
        WiringViewModel * m,
        {
            m->rx_pin = rx_pin;
            m->tx_pin = tx_pin;
        },
        true);
}
