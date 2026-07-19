#include "selftest_view.h"

#include <gui/elements.h>
#include <string.h>

typedef struct {
    bool running;
    uint8_t percent;
    uint8_t rx_pin;
    uint8_t tx_pin;
    bool have_result;
    SelfTestResult result;
} SelfTestViewModel;

struct SelfTestView {
    View* view;
    SelfTestViewCallback callback;
    void* context;
};

/* ---------------------------------------------------------------- draw ---- */

/** The idle screen is really an instruction: the test is meaningless without
 *  the jumper, so the jumper is what gets drawn. */
static void selftest_draw_idle(Canvas* canvas, const SelfTestViewModel* m) {
    char buf[24];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, "Bridge these two pins");
    canvas_draw_str(canvas, 2, 32, "with a jumper wire:");

    /* the two pads and the loop between them */
    const int y = 46;
    const int xl = 26, xr = 86;

    canvas_draw_disc(canvas, xl, y, 2);
    canvas_draw_disc(canvas, xr, y, 2);
    canvas_draw_line(canvas, xl, y - 8, xr, y - 8);
    canvas_draw_line(canvas, xl, y, xl, y - 8);
    canvas_draw_line(canvas, xr, y, xr, y - 8);

    snprintf(buf, sizeof(buf), "%u", m->tx_pin);
    canvas_draw_str_aligned(canvas, xl, y + 12, AlignCenter, AlignBottom, buf);
    snprintf(buf, sizeof(buf), "%u", m->rx_pin);
    canvas_draw_str_aligned(canvas, xr, y + 12, AlignCenter, AlignBottom, buf);

    canvas_draw_str(canvas, xl - 22, y + 3, "TX");
    canvas_draw_str(canvas, xr + 8, y + 3, "RX");

    elements_button_center(canvas, "Test");
}

static void selftest_draw_running(Canvas* canvas, const SelfTestViewModel* m) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignBottom, "Sending pattern...");
    elements_progress_bar(canvas, 2, 38, 124, (float)m->percent / 100.0f);
}

/* Four rows plus a verdict and a footer is all 54 px will hold:
 *   22      verdict (FontPrimary)
 *   30..54  the rate rows, 8 px apart - the last one ends clear of the footer
 *   63      footer (AlignBottom, so it occupies roughly 56..63)
 * Starting the rows any lower runs the last one through the footer. */
#define SELFTEST_ROW_TOP (30)
#define SELFTEST_ROW_H (8)

static void selftest_draw_result(Canvas* canvas, const SelfTestViewModel* m) {
    const SelfTestResult* r = &m->result;
    char buf[28];

    /* verdict, stated plainly - this screen exists to end an argument */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, selftest_verdict_text(r->verdict));

    /* per-rate rows: which speeds survived the cable */
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < r->completed && i < SELFTEST_STEPS; i++) {
        const int y = SELFTEST_ROW_TOP + i * SELFTEST_ROW_H;
        const SelfTestStep* s = &r->steps[i];

        snprintf(buf, sizeof(buf), "%lu", s->baud);
        canvas_draw_str(canvas, 8, y, buf);

        if(s->ok) {
            /* tick */
            canvas_draw_line(canvas, 2, y - 3, 4, y - 1);
            canvas_draw_line(canvas, 4, y - 1, 6, y - 5);
            canvas_draw_str(canvas, 60, y, "echoed");
        } else {
            /* cross */
            canvas_draw_line(canvas, 2, y - 5, 6, y - 1);
            canvas_draw_line(canvas, 6, y - 5, 2, y - 1);
            snprintf(buf, sizeof(buf), "%u/%u back", s->echoed, s->sent);
            canvas_draw_str(canvas, 60, y, buf);
        }
    }

    /* what to do about it */
    canvas_set_font(canvas, FontSecondary);
    const char* hint;
    switch(r->verdict) {
    case SelfTestPassed:
        hint = "Flipper side is fine";
        break;
    case SelfTestPartial:
        hint = "Shorten the wires";
        break;
    default:
        hint = "Check the jumper";
        break;
    }
    canvas_draw_str_aligned(canvas, 2, 63, AlignLeft, AlignBottom, hint);
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "OK: retry");
}

static void selftest_view_draw(Canvas* canvas, void* model) {
    const SelfTestViewModel* m = model;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 8, "Self Test");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, "loopback");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    if(m->running) {
        selftest_draw_running(canvas, m);
    } else if(m->have_result) {
        selftest_draw_result(canvas, m);
    } else {
        selftest_draw_idle(canvas, m);
    }
}

static bool selftest_view_input(InputEvent* event, void* context) {
    SelfTestView* sv = context;
    if(event->type != InputTypeShort || event->key != InputKeyOk) return false;

    bool busy = false;
    with_view_model(sv->view, SelfTestViewModel * m, { busy = m->running; }, false);
    if(busy) return true; // ignore, but do not let Back fire either

    if(sv->callback) sv->callback(sv->context);
    return true;
}

/* ----------------------------------------------------------------- api ---- */

SelfTestView* selftest_view_alloc(void) {
    SelfTestView* sv = malloc(sizeof(SelfTestView));
    memset(sv, 0, sizeof(SelfTestView));
    sv->view = view_alloc();
    view_allocate_model(sv->view, ViewModelTypeLocking, sizeof(SelfTestViewModel));
    view_set_context(sv->view, sv);
    view_set_draw_callback(sv->view, selftest_view_draw);
    view_set_input_callback(sv->view, selftest_view_input);
    selftest_view_reset(sv);
    return sv;
}

void selftest_view_free(SelfTestView* sv) {
    furi_assert(sv);
    view_free(sv->view);
    free(sv);
}

View* selftest_view_get_view(SelfTestView* sv) {
    furi_assert(sv);
    return sv->view;
}

void selftest_view_set_callback(SelfTestView* sv, SelfTestViewCallback cb, void* context) {
    furi_assert(sv);
    sv->callback = cb;
    sv->context = context;
}

void selftest_view_reset(SelfTestView* sv) {
    furi_assert(sv);
    with_view_model(
        sv->view,
        SelfTestViewModel * m,
        {
            memset(m, 0, sizeof(SelfTestViewModel));
            m->rx_pin = 14;
            m->tx_pin = 13;
        },
        true);
}

void selftest_view_set_pins(SelfTestView* sv, uint8_t rx_pin, uint8_t tx_pin) {
    furi_assert(sv);
    with_view_model(
        sv->view,
        SelfTestViewModel * m,
        {
            m->rx_pin = rx_pin;
            m->tx_pin = tx_pin;
        },
        true);
}

void selftest_view_set_running(SelfTestView* sv, bool running, uint8_t percent) {
    furi_assert(sv);
    with_view_model(
        sv->view,
        SelfTestViewModel * m,
        {
            m->running = running;
            m->percent = percent;
            if(running) m->have_result = false;
        },
        true);
}

void selftest_view_set_result(SelfTestView* sv, const SelfTestResult* result) {
    furi_assert(sv);
    furi_assert(result);
    with_view_model(
        sv->view,
        SelfTestViewModel * m,
        {
            m->result = *result;
            m->have_result = true;
            m->running = false;
        },
        true);
}
