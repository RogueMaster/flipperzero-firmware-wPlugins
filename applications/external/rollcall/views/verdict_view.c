#include "verdict_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

struct VerdictView {
    View* view;
    VerdictViewCallback cb;
    void* ctx;
};

typedef struct {
    RcVerdict v;
    bool has;
} VerdictModel;

/* Truncate a copy of src with an ellipsis so it fits max_w px in the font. */
static void fit_text(Canvas* canvas, const char* src, int max_w, char* out, size_t out_sz) {
    strncpy(out, src, out_sz - 1);
    out[out_sz - 1] = '\0';
    if(canvas_string_width(canvas, out) <= max_w) return;
    size_t len = strlen(out);
    while(len > 1) {
        len--;
        char probe[40];
        size_t n = (len < sizeof(probe) - 3) ? len : sizeof(probe) - 3;
        memcpy(probe, src, n);
        probe[n] = '.';
        probe[n + 1] = '.';
        probe[n + 2] = '\0';
        if(canvas_string_width(canvas, probe) <= max_w) {
            strncpy(out, probe, out_sz - 1);
            out[out_sz - 1] = '\0';
            return;
        }
    }
}

/* Greedy word-wrap into up to two lines that each fit max_w px. */
static void wrap2(Canvas* canvas, const char* src, int max_w, char* l1, char* l2, size_t sz) {
    l1[0] = l2[0] = '\0';
    const char* p = src;
    char word[40];
    char cand[96];

    while(*p) {
        while(*p == ' ') p++; // skip leading spaces
        if(!*p) break;

        size_t wl = 0; // read one word
        while(*p && *p != ' ' && wl < sizeof(word) - 1) word[wl++] = *p++;
        word[wl] = '\0';

        /* try to extend line 1 while line 2 hasn't started */
        if(l2[0] == '\0') {
            if(l1[0] == '\0')
                snprintf(cand, sizeof(cand), "%s", word);
            else
                snprintf(cand, sizeof(cand), "%s %s", l1, word);
            if(canvas_string_width(canvas, cand) <= max_w) {
                strncpy(l1, cand, sz - 1);
                l1[sz - 1] = '\0';
                continue;
            }
        }

        /* otherwise spill onto line 2 */
        if(l2[0] == '\0')
            snprintf(cand, sizeof(cand), "%s", word);
        else
            snprintf(cand, sizeof(cand), "%s %s", l2, word);
        strncpy(l2, cand, sz - 1);
        l2[sz - 1] = '\0';
    }
}

static void verdict_view_draw(Canvas* canvas, void* model) {
    VerdictModel* m = model;
    canvas_clear(canvas);
    if(!m->has) return;
    const RcVerdict* v = &m->v;

    /* --- title: the protocol --- */
    canvas_set_font(canvas, FontPrimary);
    char title[34];
    fit_text(canvas, v->protocol, 124, title, sizeof(title));
    canvas_draw_str(canvas, 2, 10, title);
    canvas_draw_line(canvas, 0, 12, 128, 12);

    /* --- grade badge (left) --- */
    canvas_draw_rframe(canvas, 2, 15, 34, 34, 3);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 19, 28, AlignCenter, AlignCenter, v->letter);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 19, 42, AlignCenter, AlignCenter, rc_class_label(v->cls));

    /* --- headline (right, up to 2 lines) --- */
    canvas_set_font(canvas, FontSecondary);
    char l1[40], l2[40];
    wrap2(canvas, v->headline, 84, l1, l2, sizeof(l1));
    canvas_draw_str(canvas, 40, 22, l1);
    if(l2[0]) canvas_draw_str(canvas, 40, 31, l2);

    /* --- replay-resistance meter (segmented) --- */
    canvas_draw_frame(canvas, 40, 35, 86, 8);
    int inner = 82; // 86 - 4
    int fill = (v->meter * inner) / 100;
    if(fill > 0) canvas_draw_box(canvas, 42, 37, fill, 4);
    for(int x = 40 + 12; x < 40 + 86; x += 12) {
        canvas_draw_line(canvas, x, 35, x, 35 + 7); // segment ticks
    }

    /* --- tally --- */
    char tally[32];
    snprintf(
        tally,
        sizeof(tally),
        "%d press%s . %d code%s",
        v->presses,
        v->presses == 1 ? "" : "es",
        v->unique,
        v->unique == 1 ? "" : "s");
    canvas_draw_str(canvas, 40, 54, tally);

    /* --- footer (inverted) --- */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 57, 128, 7);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 63, "OK: Details");
    canvas_draw_str_aligned(canvas, 125, 63, AlignRight, AlignBottom, "Retest >");
    canvas_set_color(canvas, ColorBlack);
}

static bool verdict_view_input(InputEvent* event, void* context) {
    VerdictView* v = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        if(v->cb) v->cb(v->ctx, VerdictEventDetails);
        return true;
    }
    if(event->key == InputKeyRight) {
        if(v->cb) v->cb(v->ctx, VerdictEventRescan);
        return true;
    }
    return false; // Back falls through to navigation
}

VerdictView* verdict_view_alloc(void) {
    VerdictView* v = malloc(sizeof(VerdictView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(VerdictModel));
    view_set_draw_callback(v->view, verdict_view_draw);
    view_set_input_callback(v->view, verdict_view_input);
    return v;
}

void verdict_view_free(VerdictView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* verdict_view_get_view(VerdictView* v) {
    furi_assert(v);
    return v->view;
}

void verdict_view_set_callback(VerdictView* v, VerdictViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = context;
}

void verdict_view_set_verdict(VerdictView* v, const RcVerdict* verdict) {
    furi_assert(v);
    with_view_model(
        v->view,
        VerdictModel * m,
        {
            m->v = *verdict;
            m->has = true;
        },
        true);
}
