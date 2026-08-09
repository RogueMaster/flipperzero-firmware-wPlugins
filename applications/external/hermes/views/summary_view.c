#include "summary_view.h"

#include <gui/elements.h>
#include <string.h>

typedef struct {
    SessionStats stats;
} SummaryViewModel;

struct SummaryView {
    View* view;
};

/** A labelled value in a boxed tile. Four of these tile the screen, so a whole
 *  session reads at a glance instead of as a paragraph. */
static void
    summary_tile(Canvas* canvas, int x, int y, int w, int h, const char* label, const char* value) {
    elements_slightly_rounded_frame(canvas, x, y, w, h);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x + 4, y + 9, label);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x + 4, y + h - 4, value);
}

static void summary_view_draw(Canvas* canvas, void* model) {
    const SummaryViewModel* m = model;
    const SessionStats* s = &m->stats;
    char scratch[24];
    char value[sizeof(scratch) + 4]; // room for a "%s/s" wrap of scratch

    canvas_clear(canvas);

    /* header: the link this was, and a one-word health read */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 8, "Session");
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        value, sizeof(value), "%lu %s", (unsigned long)s->baud, hermes_framing_name(s->framing));
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, value);
    canvas_draw_line(canvas, 0, 10, 127, 10);

    if(session_stats_silent(s)) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "No traffic");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "the line stayed quiet");
        return;
    }

    /* four tiles: received, throughput, errors, duration */
    const int y0 = 13, tw = 61, th = 20, gap = 2;

    session_stats_format_bytes(s->rx_bytes, scratch, sizeof(scratch));
    summary_tile(canvas, 2, y0, tw, th, "received", scratch);

    session_stats_format_bytes(session_stats_bps(s), scratch, sizeof(scratch));
    snprintf(value, sizeof(value), "%s/s", scratch);
    summary_tile(canvas, 2 + tw + gap, y0, tw, th, "throughput", value);

    snprintf(value, sizeof(value), "%lu", (unsigned long)s->errors);
    summary_tile(canvas, 2, y0 + th + gap, tw, th, "errors", value);

    session_stats_format_duration(s, scratch, sizeof(scratch));
    summary_tile(canvas, 2 + tw + gap, y0 + th + gap, tw, th, "duration", scratch);

    /* footer: the verdict, plus the log filename or the trigger count if either
     * is worth carrying out of the session */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, session_stats_verdict(s));

    if(s->logged && s->log_name[0]) {
        canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, s->log_name);
    } else if(s->trigger_hits > 0) {
        snprintf(value, sizeof(value), "%lu hits", (unsigned long)s->trigger_hits);
        canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, value);
    }
}

static bool summary_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; // Back returns to the menu; nothing else to do here
}

SummaryView* summary_view_alloc(void) {
    SummaryView* sv = malloc(sizeof(SummaryView));
    sv->view = view_alloc();
    view_allocate_model(sv->view, ViewModelTypeLocking, sizeof(SummaryViewModel));
    view_set_context(sv->view, sv);
    view_set_draw_callback(sv->view, summary_view_draw);
    view_set_input_callback(sv->view, summary_view_input);
    return sv;
}

void summary_view_free(SummaryView* sv) {
    furi_assert(sv);
    view_free(sv->view);
    free(sv);
}

View* summary_view_get_view(SummaryView* sv) {
    furi_assert(sv);
    return sv->view;
}

void summary_view_set(SummaryView* sv, const SessionStats* stats) {
    furi_assert(sv);
    furi_assert(stats);
    with_view_model(sv->view, SummaryViewModel * m, { m->stats = *stats; }, true);
}
